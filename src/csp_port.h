

#pragma once

#include <csp/csp_types.h>

csp_socket_t * csp_port_get_socket(unsigned int dport);
csp_callback_t csp_port_get_callback(unsigned int port);

int csp_unbind_port(unsigned int port);
bool csp_port_in_use(unsigned int port);
uint8_t csp_port_get_dyn(void);
uint8_t csp_port_get_dyn_free(void);
