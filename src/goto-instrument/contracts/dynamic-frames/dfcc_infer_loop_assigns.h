/*******************************************************************\

Module: Dynamic frame condition checking

Author: Remi Delmas, delmasrd@amazon.com

\*******************************************************************/

/// \file
/// Infer a set of assigns clause targets for a natural loop.

#ifndef CPROVER_GOTO_INSTRUMENT_CONTRACTS_DYNAMIC_FRAMES_DFCC_INFER_LOOP_ASSIGNS_H
#define CPROVER_GOTO_INSTRUMENT_CONTRACTS_DYNAMIC_FRAMES_DFCC_INFER_LOOP_ASSIGNS_H

#include <analyses/local_may_alias.h>
#include <goto-instrument/loop_utils.h>

#include "dfcc_loop_nesting_graph.h"

class source_locationt;
class messaget;
class namespacet;
class message_handlert;

/// Collect identifiers that are local to `loop`.
std::unordered_set<irep_idt> gen_loop_locals_set(
  const irep_idt &function_id,
  goto_functiont &goto_function,
  const dfcc_loop_nesting_graph_nodet &loop,
  message_handlert &message_handler,
  const namespacet &ns);

/// \brief Infer assigns clause targets for loops in `goto_function` from their
/// instructions and an alias analysis at the function level (with inlining),
/// and store the result in `inferred_loop_assigns_map`, a map from loop
/// numbers to the set of inferred assigns targets.
void dfcc_infer_loop_assigns_for_function(
  std::map<std::size_t, assignst> &inferred_loop_assigns_map,
  goto_functionst &goto_functions,
  const goto_functiont &goto_function,
  message_handlert &message_handler,
  const namespacet &ns);

#endif
