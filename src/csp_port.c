

#include "csp_port.h"

#include <stdlib.h>
#include <string.h>

#include <csp/csp.h>
#include <csp/csp_id.h>
#include <csp/csp_debug.h>
#include <csp/arch/csp_queue.h>
#include <csp/arch/csp_rand.h>
#include <csp_autoconfig.h>

#include "csp_conn.h"

typedef enum {
    PORT_CLOSED     = 0,
    PORT_OPEN       = 1,
    PORT_OPEN_CB    = 2,
} csp_port_state_t;

typedef struct csp_port {
    union {
        csp_socket_t *socket;
        csp_callback_t callback;
    };
    csp_port_state_t state;
    uint8_t port;
} csp_port_t;

/* We rely on the .bss section to clear this, so there is no csp_port_init() function */
static csp_port_t ports[CSP_PORT_MAX_BIND] = { 0 };

bool csp_port_in_use(unsigned int port)
{
    for (uint8_t i = 0; i < CSP_PORT_MAX_BIND; i++) {
        if (ports[i].port == port) {
            return true;
        }
    }
    return false;
}

uint8_t csp_port_get_dyn_free(void)
{
    unsigned num = (csp_id_get_max_port() - 1);
    unsigned count = num;

    do {
        uint8_t port = 1 + (csp_rand8() % num);
        if (!csp_port_in_use(port)) {
            return port;
        }
        --count;
    } while (count > 0);
    return CSP_PORT_UNSET;
}

uint8_t csp_port_get_dyn(void)
{
    return 1 + (csp_rand8() % (csp_id_get_max_port() - 1));
}

csp_port_t *csp_port_get(unsigned int port)
{
    if (port > csp_id_get_max_port() && port != CSP_PORT_ANY) {
        return NULL;
    }
    for (uint8_t i = 0; i < CSP_PORT_MAX_BIND; i++) {
        /* match for port or CSP_PORT_ANY */
        if (ports[i].port == port || ports[i].port == CSP_PORT_ANY) {
            if (ports[i].state != PORT_CLOSED) {
                return &ports[i];
            }
            return NULL;
        }
    }
    return NULL;
}

csp_callback_t csp_port_get_callback(unsigned int port)
{
    csp_port_t *port_ptr = csp_port_get(port);

    if (port_ptr) {
        return (port_ptr->state == PORT_OPEN_CB) ? port_ptr->callback : NULL;
    }
    return NULL;
}

csp_socket_t *csp_port_get_socket(unsigned int port)
{
    csp_port_t *port_ptr = csp_port_get(port);

    if (port_ptr) {
        return (port_ptr->state == PORT_OPEN) ? port_ptr->socket : NULL;
    }
    return NULL;
}

int csp_listen(csp_socket_t *socket, size_t backlog)
{
    socket->rx_queue = csp_queue_create_static(CSP_CONN_RXQUEUE_LEN, sizeof(csp_packet_t *),
                                               socket->rx_queue_static_data,
                                               &socket->rx_queue_static);
    return CSP_ERR_NONE;
}

int csp_bind(csp_socket_t *socket, uint8_t port)
{
    if (socket == NULL) {
        csp_dbg_errno = CSP_DBG_ERR_INVALID_POINTER;
        return CSP_ERR_INVAL;
    }

    if (csp_port_in_use(port)) {
        csp_dbg_errno = CSP_DBG_ERR_PORT_ALREADY_IN_USE;
        return CSP_ERR_USED;
    }

    if (port > csp_id_get_max_port() && port != CSP_PORT_ANY) {
        return CSP_ERR_INVAL;
    }

    for (uint8_t i = 0; i < CSP_PORT_MAX_BIND; i++) {
        if (ports[i].state == PORT_CLOSED) {
            ports[i].socket = socket;
            ports[i].state = PORT_OPEN;
            ports[i].port = port;
            return CSP_ERR_NONE;
        }
    }
    return CSP_ERR_NOMEM;
}

int csp_bind_callback(csp_callback_t callback, uint8_t port)
{
    if (callback == NULL) {
        csp_dbg_errno = CSP_DBG_ERR_INVALID_POINTER;
        return CSP_ERR_INVAL;
    }

    if (csp_port_in_use(port)) {
        csp_dbg_errno = CSP_DBG_ERR_PORT_ALREADY_IN_USE;
        return CSP_ERR_USED;
    }

    if (port > csp_id_get_max_port() && port != CSP_PORT_ANY) {
        return CSP_ERR_INVAL;
    }

    for (uint8_t i = 0; i < CSP_PORT_MAX_BIND; i++) {
        if (ports[i].state == PORT_CLOSED) {
            ports[i].callback = callback;
            ports[i].state = PORT_OPEN_CB;
            ports[i].port = port;
            return CSP_ERR_NONE;
        }
    }
    return CSP_ERR_NOMEM;
}

int csp_unbind_port(unsigned int port)
{
    if (port > csp_id_get_max_port() && port != CSP_PORT_ANY) {
        return CSP_ERR_INVAL;
    }

    csp_port_t *port_ptr = csp_port_get(port);

    if (port_ptr) {
        if (port_ptr->state == PORT_OPEN) {
            if (port_ptr->socket->rx_queue) {
                csp_packet_t *packet;
                while (csp_queue_dequeue(port_ptr->socket->rx_queue, &packet, 0) == CSP_QUEUE_OK) {
                    if (packet != NULL) {
                        csp_buffer_free(packet);
                    }
                }
            }
        }
        port_ptr->state = PORT_CLOSED;
        port_ptr->socket = NULL;
        port_ptr->port = CSP_PORT_UNSET;
    }
    return CSP_ERR_NONE;
}
