/*******************************************************************\

Module: Unit tests for satcheck_cadical

Author: Peter Schrammel, Michael Tautschnig

\*******************************************************************/

/// \file
/// Unit tests for satcheck_cadical

#ifdef HAVE_CADICAL

#  include <testing-utils/use_catch.h>

#  include <solvers/prop/literal.h>
#  include <solvers/sat/satcheck_cadical.h>
#  include <util/cout_message.h>

SCENARIO("satcheck_cadical", "[core][solvers][sat][satcheck_cadical]")
{
  console_message_handlert message_handler;

  GIVEN("A satisfiable formula f")
  {
    satcheck_cadical_no_preprocessingt satcheck(message_handler);
    literalt f = satcheck.new_variable();
    satcheck.l_set_to_true(f);

    THEN("is indeed satisfiable")
    {
      REQUIRE(satcheck.prop_solve() == propt::resultt::P_SATISFIABLE);
    }
    THEN("is unsatisfiable under a false assumption")
    {
      bvt assumptions;
      assumptions.push_back(const_literal(false));
      REQUIRE(
        satcheck.prop_solve(assumptions) == propt::resultt::P_UNSATISFIABLE);
    }
  }

  GIVEN("An unsatisfiable formula f && !f")
  {
    satcheck_cadical_no_preprocessingt satcheck(message_handler);
    literalt f = satcheck.new_variable();
    satcheck.l_set_to_true(satcheck.land(f, !f));

    THEN("is indeed unsatisfiable")
    {
      REQUIRE(satcheck.prop_solve() == propt::resultt::P_UNSATISFIABLE);
    }
  }

  GIVEN("An unsatisfiable formula false implied by a")
  {
    satcheck_cadical_no_preprocessingt satcheck(message_handler);
    literalt a = satcheck.new_variable();
    literalt a_implies_false = satcheck.lor(!a, const_literal(false));
    satcheck.l_set_to_true(a_implies_false);

    THEN("is indeed unsatisfiable under assumption a")
    {
      bvt assumptions;
      assumptions.push_back(a);
      REQUIRE(
        satcheck.prop_solve(assumptions) == propt::resultt::P_UNSATISFIABLE);
    }
    THEN("is still unsatisfiable under assumption a and true")
    {
      bvt assumptions;
      assumptions.push_back(const_literal(true));
      assumptions.push_back(a);
      REQUIRE(
        satcheck.prop_solve(assumptions) == propt::resultt::P_UNSATISFIABLE);
    }
    THEN("becomes satisfiable when assumption a is lifted")
    {
      bvt assumptions;
      REQUIRE(
        satcheck.prop_solve(assumptions) == propt::resultt::P_SATISFIABLE);
    }
  }
}

#endif
