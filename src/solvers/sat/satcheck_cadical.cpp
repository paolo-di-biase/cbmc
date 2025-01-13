/*******************************************************************\

Module:

Author: Michael Tautschnig

\*******************************************************************/

#ifdef HAVE_CADICAL

#  include "satcheck_cadical.h"

#  include <util/exception_utils.h>
#  include <util/invariant.h>
#  include <util/narrow.h>
#  include <util/threeval.h>

#  include <cadical.hpp>

tvt satcheck_cadical_baset::l_get(literalt a) const
{
  if(a.is_constant())
    return tvt(a.sign());

  tvt result;

  if(a.var_no() > narrow<unsigned>(solver->vars()))
    return tvt(tvt::tv_enumt::TV_UNKNOWN);

  const int val = solver->val(a.dimacs());
  if(val>0)
    result = tvt(true);
  else if(val<0)
    result = tvt(false);
  else
    return tvt(tvt::tv_enumt::TV_UNKNOWN);

  return result;
}

std::string satcheck_cadical_baset::solver_text() const
{
  return std::string("CaDiCaL ") + solver->version();
}

void satcheck_cadical_baset::lcnf(const bvt &bv)
{
  for(const auto &lit : bv)
  {
    if(lit.is_true())
      return;
    else if(!lit.is_false())
      INVARIANT(lit.var_no() < no_variables(), "reject out of bound variables");
  }

  for(const auto &lit : bv)
  {
    if(!lit.is_false())
    {
      // add literal with correct sign
      solver->add(lit.dimacs());
    }
  }
  solver->add(0); // terminate clause

  if(solver_hardness)
  {
    // To map clauses to lines of program code, track clause indices in the
    // dimacs cnf output. Dimacs output is generated after processing
    // clauses to remove duplicates and clauses that are trivially true.
    // Here, a clause is checked to see if it can be thus eliminated. If
    // not, add the clause index to list of clauses in
    // solver_hardnesst::register_clause().
    static size_t cnf_clause_index = 0;
    bvt cnf;
    bool clause_removed = process_clause(bv, cnf);

    if(!clause_removed)
      cnf_clause_index++;

    solver_hardness->register_clause(
      bv, cnf, cnf_clause_index, !clause_removed);
  }

  clause_counter++;
}

propt::resultt satcheck_cadical_baset::do_prop_solve(const bvt &assumptions)
{
  INVARIANT(status != statust::ERROR, "there cannot be an error");

  log.statistics() << (no_variables() - 1) << " variables, " << clause_counter
                   << " clauses" << messaget::eom;

  // if assumptions contains false, we need this to be UNSAT
  for(const auto &a : assumptions)
  {
    if(a.is_false())
    {
      log.status() << "got FALSE as assumption: instance is UNSATISFIABLE"
                   << messaget::eom;
      status = statust::UNSAT;
      return resultt::P_UNSATISFIABLE;
    }
  }

  for(const auto &a : assumptions)
    if(!a.is_true())
      solver->assume(a.dimacs());

  // set preprocessing and inprocessing limits
  auto limit1_ret = solver->limit("preprocessing", preprocessing_limit);
  CHECK_RETURN(limit1_ret);
  auto limit2_ret = solver->limit("localsearch", localsearch_limit);
  CHECK_RETURN(limit2_ret);

  switch(solver->solve())
  {
  case 10:
    log.status() << "SAT checker: instance is SATISFIABLE" << messaget::eom;
    status = statust::SAT;
    return resultt::P_SATISFIABLE;
  case 20:
    log.status() << "SAT checker: instance is UNSATISFIABLE" << messaget::eom;
    break;
  default:
    log.status() << "SAT checker: solving returned without solution"
                 << messaget::eom;
    throw analysis_exceptiont(
      "solving inside CaDiCaL SAT solver has been interrupted");
  }

  status = statust::UNSAT;
  return resultt::P_UNSATISFIABLE;
}

void satcheck_cadical_baset::set_assignment(literalt a, bool value)
{
  INVARIANT(!a.is_constant(), "cannot set an assignment for a constant");
  INVARIANT(false, "method not supported");
}

satcheck_cadical_baset::satcheck_cadical_baset(
  int _preprocessing_limit,
  int _localsearch_limit,
  message_handlert &message_handler)
  : cnf_solvert(message_handler),
    solver(new CaDiCaL::Solver()),
    preprocessing_limit(_preprocessing_limit),
    localsearch_limit(_localsearch_limit)
{
  solver->set("quiet", 1);
}

satcheck_cadical_baset::~satcheck_cadical_baset()
{
  delete solver;
}

bool satcheck_cadical_baset::is_in_conflict(literalt a) const
{
  return solver->failed(a.dimacs());
}

#endif
