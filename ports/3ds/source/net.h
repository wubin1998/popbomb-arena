#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Two consoles run the same deterministic simulation and exchange only their
   own input. Inputs are applied NET_INPUT_DELAY ticks late on both sides, so a
   packet that arrives one tick after it was sampled still lands in time. */
#define NET_PORT 4242
#define NET_VERSION 1
#define NET_INPUT_DELAY 2

enum { NET_OFF = 0, NET_HOST, NET_CLIENT };

typedef struct __attribute__((packed)) {
	uint32_t seed;
	uint8_t seq;
	uint8_t scene, stage, total_stage, diff, players;
	uint8_t hero[2], lives[2], pair[2];
} NetStage;

int net_role(void);
int net_local_slot(void);

bool net_open(int role);
void net_close(void);
void net_poll(void);

const char* net_status(void);
const char* net_error(void);
const char* net_local_ip(void);

bool net_peer_joined(void);
int net_peer_hero(void);
bool net_take_disconnect(void);

/* host sends; the client consumes the queued stage */
void net_send_stage(const NetStage* st);
bool net_recv_stage(NetStage* st);
void net_reset_ticks(void);

void net_prepare_tick(int tick);
bool net_tick_ready(int tick);
void net_set_sim_tick(int tick);
uint8_t net_input_bits(int slot);

void net_send_bye(void);
void net_set_hero(int id);
