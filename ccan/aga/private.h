/* Licensed under LGPLv2+ - see LICENSE file for details */
#ifndef CCAN_AGA_PRIVATE_H
#define CCAN_AGA_PRIVATE_H

int aga_start(struct aga_graph *g);
bool aga_node_needs_update(const struct aga_graph *g,
			   const struct aga_node *node);
bool aga_update_node(const struct aga_graph *g, struct aga_node *node);
bool aga_check_state(const struct aga_graph *g);
void aga_fail(struct aga_graph *g, int error);

#endif /* CCAN_AGA_PRIVATE_H */
