/*******************************************************************\

Module: Nondeterministically initializes global scope variables, except for
 constants (such as string literals, final fields) and internal variables
 (such as CPROVER and symex variables, language specific internal
 variables).

Author: Daniel Kroening, Michael Tautschnig

Date: November 2011

\*******************************************************************/

/// \file
/// Nondeterministically initializes global scope variables, except for
/// constants (such as string literals, final fields) and internal variables
/// (such as CPROVER and symex variables, language specific internal
/// variables).

#include "nondet_static.h"

#include <util/prefix.h>
#include <util/std_code.h>

#include <goto-programs/goto_model.h>

#include <linking/static_lifetime_init.h>

#include <regex>

/// See the return.
/// \param symbol_expr: The symbol expression to analyze.
/// \param ns: Namespace for resolving type information
/// \return True if the symbol expression holds a static symbol which can be
///   nondeterministically initialized, false otherwise.
bool is_nondet_initializable_static(
  const symbol_exprt &symbol_expr,
  const namespacet &ns)
{
  const irep_idt &id = symbol_expr.get_identifier();

  // is it a __CPROVER_* variable?
  if(id.starts_with(CPROVER_PREFIX))
    return false;

  // variable not in symbol table such as symex variable?
  if(!ns.get_symbol_table().has_symbol(id))
    return false;

  const symbolt &symbol = ns.lookup(id);

  // is the symbol explicitly marked as not to be nondet initialized?
  if(symbol.value.get_bool(ID_C_no_nondet_initialization))
  {
    return false;
  }

  // is the type explicitly marked as not to be nondet initialized?
  if(symbol.type.get_bool(ID_C_no_nondet_initialization))
    return false;

  // is the type explicitly marked as not to be initialized?
  if(symbol.type.get_bool(ID_C_no_initialization_required))
    return false;

  // static lifetime?
  if(!symbol.is_static_lifetime)
    return false;

  // constant?
  return !is_constant_or_has_constant_components(symbol_expr.type(), ns) &&
         !is_constant_or_has_constant_components(symbol.type, ns);
}

/// Nondeterministically initializes global scope variables in a goto-function.
/// Iterates over instructions in the specified function and replaces all values
/// assigned to nondet-initializable static variables with nondeterministic
/// values.
/// \param ns: Namespace for resolving type information.
/// \param [inout] goto_model: Existing goto-functions and symbol table to
///   be updated.
/// \param fct_name: Name of the goto-function to be updated.
static void nondet_static(
  const namespacet &ns,
  goto_modelt &goto_model,
  const irep_idt &fct_name)
{
  goto_functionst::function_mapt::iterator fct_entry =
    goto_model.goto_functions.function_map.find(fct_name);
  CHECK_RETURN(fct_entry != goto_model.goto_functions.function_map.end());

  goto_programt &init = fct_entry->second.body;

  for(auto &instruction : init.instructions)
  {
    if(instruction.is_assign())
    {
      const symbol_exprt sym =
        to_symbol_expr(as_const(instruction).assign_lhs());

      if(is_nondet_initializable_static(sym, ns))
      {
        side_effect_expr_nondett nondet{
          sym.type(), instruction.source_location()};
        instruction.assign_rhs_nonconst() = nondet;
        goto_model.symbol_table.get_writeable_ref(sym.get_identifier()).value =
          nondet;
      }
    }
    else if(instruction.is_function_call())
    {
      const symbol_exprt &fsym = to_symbol_expr(instruction.call_function());

      // see cpp/cpp_typecheck.cpp, which creates initialization functions
      if(fsym.get_identifier().starts_with("#cpp_dynamic_initialization#"))
      {
        nondet_static(ns, goto_model, fsym.get_identifier());
      }
    }
  }

  // update counters etc.
  goto_model.goto_functions.update();
}

/// First main entry point of the module. Nondeterministically initializes
/// global scope variables, except for constants (such as string literals, final
/// fields) and internal variables (such as CPROVER and symex variables,
/// language specific internal variables).
/// \param [inout] goto_model: Existing goto-model to be updated.
void nondet_static(goto_modelt &goto_model)
{
  const namespacet ns(goto_model.symbol_table);
  nondet_static(ns, goto_model, INITIALIZE_FUNCTION);
}

/// Second main entry point of the module. Nondeterministically initializes
/// global scope variables, except for constants (such as string literals, final
/// fields), internal variables (such as CPROVER and symex variables,
/// language specific internal variables) and variables passed to except_value.
/// \param [out] goto_model: Existing goto-model to be updated.
/// \param except_values: list of symbol names that should not be updated.
void nondet_static(
  goto_modelt &goto_model,
  const std::set<std::string> &except_values)
{
  const namespacet ns(goto_model.symbol_table);
  std::set<std::string> to_exclude;

  for(auto const &except : except_values)
  {
    const bool file_prefix_found = except.find(":") != std::string::npos;

    if(file_prefix_found)
    {
      to_exclude.insert(except);
      if(has_prefix(except, "./"))
      {
        to_exclude.insert(except.substr(2, except.length() - 2));
      }
      else
      {
        to_exclude.insert("./" + except);
      }
    }
    else
    {
      irep_idt symbol_name(except);
      symbolt lookup_results = ns.lookup(symbol_name);
      to_exclude.insert(
        id2string(lookup_results.location.get_file()) + ":" + except);
    }
  }

  symbol_tablet &symbol_table = goto_model.symbol_table;

  for(symbol_tablet::iteratort symbol_it = symbol_table.begin();
      symbol_it != symbol_table.end();
      symbol_it++)
  {
    symbolt &symbol = symbol_it.get_writeable_symbol();
    std::string qualified_name = id2string(symbol.location.get_file()) + ":" +
                                 id2string(symbol.display_name());
    if(to_exclude.find(qualified_name) != to_exclude.end())
    {
      symbol.value.set(ID_C_no_nondet_initialization, 1);
    }
    else if(is_nondet_initializable_static(symbol.symbol_expr(), ns))
    {
      symbol.value = side_effect_expr_nondett(symbol.type, symbol.location);
    }
  }

  nondet_static(ns, goto_model, INITIALIZE_FUNCTION);
}

/// Nondeterministically initializes global scope variables that
/// match the given regex.
/// \param [out] goto_model: Existing goto-model to be updated.
/// \param regex: regex for matching variables in the format
///   "filename:variable" (same format as those of except_values in
///   nondet_static(goto_model, except_values) variant above).
void nondet_static_matching(goto_modelt &goto_model, const std::string &regex)
{
  const auto regex_matcher = std::regex(regex);
  symbol_tablet &symbol_table = goto_model.symbol_table;

  for(symbol_tablet::iteratort symbol_it = symbol_table.begin();
      symbol_it != symbol_table.end();
      symbol_it++)
  {
    symbolt &symbol = symbol_it.get_writeable_symbol();
    std::string qualified_name = id2string(symbol.location.get_file()) + ":" +
                                 id2string(symbol.display_name());
    if(!std::regex_match(qualified_name, regex_matcher))
    {
      symbol.value.set(ID_C_no_nondet_initialization, 1);
    }
  }

  const namespacet ns(goto_model.symbol_table);
  nondet_static(ns, goto_model, INITIALIZE_FUNCTION);
}
