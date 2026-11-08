#include "net.h"

#include <3ds.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define HIST 64
#define KEEPALIVE_MS 1000
#define TIMEOUT_MS 6000
#define RETRY_MS 600

enum {
	PKT_DISCOVER = 'D',
	PKT_REPLY = 'R',
	PKT_JOIN = 'J',
	PKT_ACCEPT = 'A',
	PKT_STAGE = 'S',
	PKT_INPUT = 'I',
	PKT_PING = 'P',
	PKT_BYE = 'Q',
};

typedef struct __attribute__((packed)) {
	char magic[4];
	u8 type;
} PktHdr;

typedef struct __attribute__((packed)) {
	PktHdr h;
	u8 ver;
} PktHello;

typedef struct __attribute__((packed)) {
	PktHdr h;
	u8 ver;
	u8 hero;
} PktJoin;

typedef struct __attribute__((packed)) {
	PktHdr h;
	u8 ver;
	u8 slot;
} PktAccept;

typedef struct __attribute__((packed)) {
	PktHdr h;
	NetStage st;
} PktStage;

typedef struct __attribute__((packed)) {
	PktHdr h;
	u16 tick;
	u8 n;
	u8 bits[8];
} PktInput;

static u32 s_soc_mem[0x20000 / 4] __attribute__((aligned(0x1000)));
static bool s_soc_ready;

static int s_role = NET_OFF;
static int s_local_slot;
static int s_fd = -1;
static struct sockaddr_in s_peer;
static bool s_have_peer;
static bool s_joined;
static bool s_disconnect;
static bool s_stage_pending;
static NetStage s_stage;
static int s_peer_hero;
static u8 s_want_hero = 1;
static char s_status[64];
static char s_error[64];
static char s_ip[20];

static u16 s_own_tick[HIST];
static u8 s_own_bits[HIST];
static u16 s_peer_tick[HIST];
static u8 s_peer_bits[HIST];
static int s_sim_tick;

static u64 s_last_rx;
static u64 s_last_tx;
static u64 s_last_retry;

static bool s_stage_out;
static PktStage s_stage_pkt;
static u64 s_stage_tx;

static void set_status(const char* text)
{
	snprintf(s_status, sizeof(s_status), "%s", text);
}

static void set_error(const char* text)
{
	snprintf(s_error, sizeof(s_error), "%s", text);
}

static u64 now_ms(void)
{
	return osGetTime();
}

static void send_to(const struct sockaddr_in* dst, const void* data, size_t len)
{
	if (s_fd < 0)
		return;
	sendto(s_fd, data, len, 0, (const struct sockaddr*)dst, sizeof(*dst));
	s_last_tx = now_ms();
}

static void send_peer(const void* data, size_t len)
{
	if (!s_have_peer)
		return;
	send_to(&s_peer, data, len);
}

static void send_broadcast(const void* data, size_t len)
{
	struct sockaddr_in b;
	memset(&b, 0, sizeof(b));
	b.sin_family = AF_INET;
	b.sin_port = htons(NET_PORT);
	b.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	send_to(&b, data, len);
}

static void make_hdr(PktHdr* h, u8 type)
{
	memcpy(h->magic, "PBv1", 4);
	h->type = type;
}

void net_reset_ticks(void)
{
	memset(s_own_tick, 0xff, sizeof(s_own_tick));
	memset(s_peer_tick, 0xff, sizeof(s_peer_tick));
	s_sim_tick = 0;
}

static bool soc_up(void)
{
	if (s_soc_ready)
		return true;
	if (R_FAILED(socInit(s_soc_mem, sizeof(s_soc_mem)))) {
		set_error("无法连接网络");
		return false;
	}
	s_soc_ready = true;
	return true;
}

static u8 sample_input(void)
{
	u32 keys = hidKeysHeld();
	circlePosition c;
	hidCircleRead(&c);
	u8 bits = 0;
	if (keys & KEY_DLEFT || c.dx < -40)
		bits |= 1 << 0;
	if (keys & KEY_DUP || c.dy > 40)
		bits |= 1 << 1;
	if (keys & KEY_DRIGHT || c.dx > 40)
		bits |= 1 << 2;
	if (keys & KEY_DDOWN || c.dy < -40)
		bits |= 1 << 3;
	if (keys & (KEY_A | KEY_X))
		bits |= 1 << 4;
	return bits;
}

static void send_input(int tick)
{
	if (!s_joined)
		return;
	PktInput p;
	make_hdr(&p.h, PKT_INPUT);
	p.tick = (u16)tick;
	p.n = 8;
	for (int i = 0; i < 8; i++) {
		int t = tick - (7 - i);
		if (t < 0) {
			p.bits[i] = 0;
			continue;
		}
		int idx = t & (HIST - 1);
		p.bits[i] = s_own_tick[idx] == (u16)t ? s_own_bits[idx] : 0;
	}
	send_peer(&p, sizeof(p));
}

static void handle_packet(const u8* buf, int len, const struct sockaddr_in* from)
{
	u8 type = buf[4];

	if (type == PKT_DISCOVER && s_role == NET_HOST) {
		PktHello p;
		make_hdr(&p.h, PKT_REPLY);
		p.ver = NET_VERSION;
		send_to(from, &p, sizeof(p));
		return;
	}

	if (type == PKT_REPLY && s_role == NET_CLIENT && !s_joined) {
		s_peer = *from;
		s_have_peer = true;
		PktJoin j;
		memset(&j, 0, sizeof(j));
		make_hdr(&j.h, PKT_JOIN);
		j.ver = NET_VERSION;
		j.hero = s_want_hero;
		send_peer(&j, sizeof(j));
		s_last_retry = now_ms();
		set_status("正在连接主机…");
		return;
	}

	if (type == PKT_JOIN && s_role == NET_HOST && !s_joined) {
		if (len < (int)sizeof(PktJoin))
			return;
		s_peer = *from;
		s_have_peer = true;
		s_peer_hero = buf[6];
		PktAccept a;
		make_hdr(&a.h, PKT_ACCEPT);
		a.ver = NET_VERSION;
		a.slot = 2;
		send_peer(&a, sizeof(a));
		s_joined = true;
		s_last_rx = now_ms();
		set_status("玩家已连接");
		return;
	}

	if (type == PKT_ACCEPT && s_role == NET_CLIENT && !s_joined) {
		if (len < (int)sizeof(PktAccept) || buf[6] != 2)
			return;
		s_joined = true;
		s_last_rx = now_ms();
		set_status("已连接，等待主机开始");
		return;
	}

	/* A stage means the host accepted us even if its ACCEPT was lost. */
	if (type == PKT_STAGE && s_role == NET_CLIENT) {
		if (len < (int)sizeof(PktStage))
			return;
		if (!s_joined) {
			s_joined = true;
			s_peer = *from;
			s_have_peer = true;
			set_status("已连接，等待主机开始");
		}
		s_last_rx = now_ms();
		memcpy(&s_stage, buf + sizeof(PktHdr), sizeof(s_stage));
		s_stage_pending = true;
		return;
	}

	if (!s_joined)
		return;

	if (type == PKT_BYE) {
		s_disconnect = true;
		set_status(s_role == NET_CLIENT ? "主机已退出" : "玩家已退出");
		return;
	}

	s_last_rx = now_ms();

	if (type == PKT_INPUT) {
		if (len < (int)sizeof(PktInput))
			return;
		u16 tick;
		memcpy(&tick, buf + sizeof(PktHdr), sizeof(tick));
		u8 n = buf[sizeof(PktHdr) + 2];
		if (n > 8)
			n = 8;
		/* Only an input from the new stage proves the peer got the packet;
		   late inputs from the previous stage must not stop the retries. */
		if (n > 0 && tick >= 1 && (int)tick - (n - 1) <= 1)
			s_stage_out = false;
		const u8* bits = buf + sizeof(PktHdr) + 3;
		for (int i = 0; i < n; i++) {
			int t = (int)tick - (n - 1 - i);
			if (t < 0)
				continue;
			int idx = t & (HIST - 1);
			s_peer_tick[idx] = (u16)t;
			s_peer_bits[idx] = bits[i];
		}
	}
}

int net_role(void)
{
	return s_role;
}

int net_local_slot(void)
{
	return s_local_slot;
}

bool net_open(int role)
{
	net_close();
	memset(s_error, 0, sizeof(s_error));
	memset(s_ip, 0, sizeof(s_ip));
	net_reset_ticks();
	s_disconnect = false;
	s_joined = false;
	s_have_peer = false;
	s_stage_pending = false;
	s_stage_out = false;
	s_peer_hero = 0;

	if (!soc_up())
		return false;

	s_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (s_fd < 0) {
		set_error("无法创建网络连接");
		return false;
	}

	int one = 1;
	setsockopt(s_fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
	setsockopt(s_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(NET_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(s_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		close(s_fd);
		s_fd = -1;
		set_error("端口 4242 被占用");
		return false;
	}

	int fl = fcntl(s_fd, F_GETFL, 0);
	fcntl(s_fd, F_SETFL, fl | O_NONBLOCK);

	struct in_addr self;
	self.s_addr = (u32)gethostid();
	snprintf(s_ip, sizeof(s_ip), "%s", inet_ntoa(self));

	s_role = role;
	s_local_slot = role == NET_HOST ? 1 : 2;
	s_last_rx = now_ms();
	s_last_tx = 0;
	s_last_retry = 0;
	set_status(role == NET_HOST ? "等待玩家加入" : "正在搜索主机…");
	return true;
}

void net_close(void)
{
	if (s_fd >= 0) {
		close(s_fd);
		s_fd = -1;
	}
	s_role = NET_OFF;
	s_local_slot = 0;
	s_joined = false;
	s_have_peer = false;
	s_stage_pending = false;
	s_stage_out = false;
	s_disconnect = false;
	s_status[0] = '\0';
}

void net_send_bye(void)
{
	if (s_role == NET_OFF || !s_joined)
		return;
	PktHdr p;
	make_hdr(&p, PKT_BYE);
	send_peer(&p, sizeof(p));
}

void net_poll(void)
{
	if (s_role == NET_OFF || s_fd < 0)
		return;

	u64 now = now_ms();

	if (!s_joined && s_role == NET_CLIENT) {
		if (now - s_last_retry >= RETRY_MS) {
			s_last_retry = now;
			if (s_have_peer) {
				PktJoin j;
				memset(&j, 0, sizeof(j));
				make_hdr(&j.h, PKT_JOIN);
				j.ver = NET_VERSION;
				j.hero = s_want_hero;
				send_peer(&j, sizeof(j));
			} else {
				PktHello p;
				make_hdr(&p.h, PKT_DISCOVER);
				p.ver = NET_VERSION;
				send_broadcast(&p, sizeof(p));
			}
		}
	}

	if (s_joined && now - s_last_rx >= TIMEOUT_MS) {
		s_disconnect = true;
		set_status("连接已断开");
	}

	for (;;) {
		u8 buf[128];
		struct sockaddr_in from;
		socklen_t fl = sizeof(from);
		int n = recvfrom(s_fd, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fl);
		if (n <= 0)
			break;
		if (n < (int)sizeof(PktHdr) || memcmp(buf, "PBv1", 4) != 0)
			continue;
		handle_packet(buf, n, &from);
	}

	if (s_joined && now - s_last_tx >= KEEPALIVE_MS) {
		PktHdr p;
		make_hdr(&p, PKT_PING);
		send_peer(&p, sizeof(p));
	}

	/* Until the peer answers with a tick, repeat the stage so one lost
	   datagram cannot leave the client stuck in the lobby. */
	if (s_role == NET_HOST && s_stage_out && now - s_stage_tx >= RETRY_MS) {
		s_stage_tx = now;
		send_peer(&s_stage_pkt, sizeof(s_stage_pkt));
	}
}

const char* net_status(void)
{
	return s_status;
}

const char* net_error(void)
{
	return s_error[0] ? s_error : NULL;
}

const char* net_local_ip(void)
{
	return s_ip;
}

bool net_peer_joined(void)
{
	return s_joined;
}

int net_peer_hero(void)
{
	return s_peer_hero;
}

bool net_take_disconnect(void)
{
	if (!s_disconnect)
		return false;
	s_disconnect = false;
	return true;
}

void net_set_hero(int id)
{
	if (id >= 1 && id <= 4)
		s_want_hero = (u8)id;
}

void net_send_stage(const NetStage* st)
{
	if (s_role != NET_HOST)
		return;
	net_reset_ticks();
	PktStage p;
	make_hdr(&p.h, PKT_STAGE);
	memcpy(&p.st, st, sizeof(*st));
	s_stage_pkt = p;
	s_stage_out = true;
	s_stage_tx = now_ms();
	send_peer(&p, sizeof(p));
}

bool net_recv_stage(NetStage* st)
{
	if (!s_stage_pending)
		return false;
	s_stage_pending = false;
	*st = s_stage;
	return true;
}

void net_prepare_tick(int tick)
{
	if (s_role == NET_OFF)
		return;
	if (tick < 0 || tick > 0xffff)
		return;
	int idx = tick & (HIST - 1);
	s_own_tick[idx] = (u16)tick;
	s_own_bits[idx] = sample_input();
	send_input(tick);
}

bool net_tick_ready(int tick)
{
	if (s_role == NET_OFF)
		return true;
	if (tick <= 0)
		return true;
	if (tick > 0xffff)
		return false;
	int idx = tick & (HIST - 1);
	return s_peer_tick[idx] == (u16)tick;
}

void net_set_sim_tick(int tick)
{
	s_sim_tick = tick;
}

uint8_t net_input_bits(int slot)
{
	if (s_role == NET_OFF)
		return 0;
	int tick = s_sim_tick - NET_INPUT_DELAY;
	if (tick <= 0)
		return 0;
	int idx = tick & (HIST - 1);
	if (slot == s_local_slot)
		return s_own_tick[idx] == (u16)tick ? s_own_bits[idx] : 0;
	return s_peer_tick[idx] == (u16)tick ? s_peer_bits[idx] : 0;
}
