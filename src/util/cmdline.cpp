/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include "cmdline.h"

#include <util/edit_distance.h>
#include <util/exception_utils.h>
#include <util/invariant.h>
#include <util/string_utils.h>

cmdlinet::cmdlinet()
{
}

cmdlinet::~cmdlinet()
{
}

void cmdlinet::clear()
{
  options.clear();
  args.clear();
}

bool cmdlinet::isset(char option) const
{
  auto i=getoptnr(option);
  if(i.has_value())
    return options[*i].isset;
  else
    return false;
}

bool cmdlinet::isset(const char *option) const
{
  auto i=getoptnr(option);
  if(i.has_value())
    return options[*i].isset;
  else
    return false;
}

std::string cmdlinet::get_value(char option) const
{
  return value_opt(option).value_or("");
}

std::optional<std::string> cmdlinet::value_opt(char option) const
{
  auto i=getoptnr(option);

  if(i.has_value() && !options[*i].values.empty())
    return options[*i].values.front();
  else
    return {};
}

void cmdlinet::set(const std::string &option, bool value)
{
  auto i=getoptnr(option);

  if(i.has_value())
    options[*i].isset = value;
  else
  {
    throw invalid_command_line_argument_exceptiont(
      "unknown command line option", option);
  }
}

void cmdlinet::set(const std::string &option, const std::string &value)
{
  auto i=getoptnr(option);

  if(i.has_value())
  {
    options[*i].isset=true;
    options[*i].values.push_back(value);
  }
  else
  {
    throw invalid_command_line_argument_exceptiont(
      "unknown command line option", option);
  }
}

static std::list<std::string> immutable_empty_list;

const std::list<std::string> &cmdlinet::get_values(char option) const
{
  auto i=getoptnr(option);

  if(i.has_value())
    return options[*i].values;
  else
    return immutable_empty_list;
}

std::string cmdlinet::get_value(const char *option) const
{
  return value_opt(option).value_or("");
}

std::optional<std::string> cmdlinet::value_opt(const char *option) const
{
  auto i=getoptnr(option);

  if(i.has_value() && !options[*i].values.empty())
    return options[*i].values.front();
  else
    return {};
}

const std::list<std::string> &cmdlinet::get_values(
  const std::string &option) const
{
  auto i=getoptnr(option);

  if(i.has_value())
    return options[*i].values;
  else
    return immutable_empty_list;
}

std::list<std::string>
cmdlinet::get_comma_separated_values(const char *option) const
{
  std::list<std::string> separated_values;

  for(const auto &csv : get_values(option))
  {
    const auto values = split_string(csv, ',');
    separated_values.insert(
      separated_values.end(), values.begin(), values.end());
  }

  return separated_values;
}

std::optional<std::size_t> cmdlinet::getoptnr(char option) const
{
  for(std::size_t i=0; i<options.size(); i++)
    if(options[i].optchar==option)
      return i;

  return std::optional<std::size_t>();
}

std::optional<std::size_t> cmdlinet::getoptnr(const std::string &option) const
{
  for(std::size_t i=0; i<options.size(); i++)
    if(options[i].optstring==option)
      return i;

  return std::optional<std::size_t>();
}

bool cmdlinet::parse(int argc, const char **argv, const char *optstring)
{
  clear();

  parse_optstring(optstring);
  return parse_arguments(argc, argv);
}

cmdlinet::option_namest cmdlinet::option_names() const
{
  return option_namest{*this};
}
void cmdlinet::parse_optstring(const char *optstring)
{
  while(optstring[0] != 0)
  {
    optiont option;

    DATA_INVARIANT(
      optstring[0] != ':', "cmdlinet::parse: Invalid option string\n");

    if(optstring[0] == '(')
    {
      option.islong = true;
      option.optchar = 0;
      option.isset = false;
      option.optstring.clear();

      for(optstring++; optstring[0] != ')' && optstring[0] != 0; optstring++)
        option.optstring += optstring[0];

      if(optstring[0] == ')')
        optstring++;
    }
    else
    {
      option.islong = false;
      option.optchar = optstring[0];
      option.optstring.clear();
      option.isset = false;

      optstring++;
    }

    if(optstring[0] == ':')
    {
      option.hasval = true;
      optstring++;
    }
    else
      option.hasval = false;

    options.push_back(option);
  }
}

std::vector<std::string>
cmdlinet::get_argument_suggestions(const std::string &unknown_argument)
{
  struct suggestiont
  {
    std::size_t distance;
    std::string suggestion;

    bool operator<(const suggestiont &other) const
    {
      return distance < other.distance;
    }
  };

  auto argument_suggestions = std::vector<suggestiont>{};
  // We allow 3 errors here. This can lead to the output being a bit chatty,
  // which we mitigate by reducing suggestions to those with the minimum
  // distance further down below
  const auto argument_matcher = levenshtein_automatont{unknown_argument, 3};
  for(const auto &option : options)
  {
    if(option.islong)
    {
      const auto long_name = "--" + option.optstring;
      if(auto distance = argument_matcher.get_edit_distance(long_name))
      {
        argument_suggestions.push_back({distance.value(), long_name});
      }
    }
    if(!option.islong)
    {
      const auto short_name = std::string{"-"} + option.optchar;
      if(auto distance = argument_matcher.get_edit_distance(short_name))
      {
        argument_suggestions.push_back({distance.value(), short_name});
      }
    }
  }

  auto final_suggestions = std::vector<std::string>{};
  if(!argument_suggestions.empty())
  {
    // we only want to keep suggestions with the minimum distance
    // because otherwise they become quickly too noisy to be useful
    auto min = std::min_element(
      argument_suggestions.begin(), argument_suggestions.end());
    INVARIANT(
      min != argument_suggestions.end(),
      "there is a minimum because it's not empty");
    for(auto const &suggestion : argument_suggestions)
    {
      if(suggestion.distance == min->distance)
      {
        final_suggestions.push_back(suggestion.suggestion);
      }
    }
  }
  return final_suggestions;
}

bool cmdlinet::parse_arguments(int argc, const char **argv)
{
  for(int i = 1; i < argc; i++)
  {
    if(argv[i][0] != '-')
      args.push_back(argv[i]);
    else
    {
      std::optional<std::size_t> optnr;

      if(argv[i][1] != 0 && argv[i][2] == 0)
        optnr = getoptnr(argv[i][1]); // single-letter option -X
      else if(argv[i][1] == '-')
        optnr = getoptnr(argv[i] + 2); // multi-letter option with --XXX
      else
      {
        // Multi-letter option -XXX, or single-letter with argument -Xval
        // We first try single-letter.
        optnr = getoptnr(argv[i][1]);

        if(!optnr.has_value()) // try multi-letter
          optnr = getoptnr(argv[i] + 1);
      }

      if(!optnr.has_value())
      {
        unknown_arg = argv[i];
        return true;
      }

      options[*optnr].isset = true;

      if(options[*optnr].hasval)
      {
        if(argv[i][2] == 0 || options[*optnr].islong)
        {
          i++;
          if(i == argc)
            return true;
          if(argv[i][0] == '-' && argv[i][1] != 0)
            return true;
          options[*optnr].values.push_back(argv[i]);
        }
        else
          options[*optnr].values.push_back(argv[i] + 2);
      }
    }
  }
  return false;
}

cmdlinet::option_namest::option_names_iteratort::option_names_iteratort(
  const cmdlinet *command_line,
  std::size_t index)
  : command_line(command_line), index(index)
{
  goto_next_valid_index();
}

cmdlinet::option_namest::option_names_iteratort &
cmdlinet::option_namest::option_names_iteratort::operator++()
{
  PRECONDITION(command_line != nullptr);
  ++index;
  goto_next_valid_index();
  return *this;
}
bool cmdlinet::option_namest::option_names_iteratort::is_valid_index() const
{
  PRECONDITION(command_line != nullptr);
  auto const &options = command_line->options;
  return index < options.size() && options[index].isset &&
         options[index].islong;
}

void cmdlinet::option_namest::option_names_iteratort::goto_next_valid_index()
{
  PRECONDITION(command_line != nullptr);
  while(index < command_line->options.size() && !is_valid_index())
  {
    ++index;
  }
}

const cmdlinet::option_namest::option_names_iteratort
cmdlinet::option_namest::option_names_iteratort::operator++(int dummy)
{
  return ++option_names_iteratort(*this);
}

const std::string &cmdlinet::option_namest::option_names_iteratort::operator*()
{
  PRECONDITION(command_line != nullptr);
  return command_line->options.at(index).optstring;
}

bool cmdlinet::option_namest::option_names_iteratort::
operator==(const cmdlinet::option_namest::option_names_iteratort &other)
{
  PRECONDITION(command_line != nullptr && command_line == other.command_line);
  return index == other.index;
}

bool cmdlinet::option_namest::option_names_iteratort::
operator!=(const cmdlinet::option_namest::option_names_iteratort &other)
{
  PRECONDITION(command_line != nullptr && command_line == other.command_line);
  return index != other.index;
}

cmdlinet::option_namest::option_namest(const cmdlinet &command_line)
  : command_line(command_line)
{
}

cmdlinet::option_namest::option_names_iteratort cmdlinet::option_namest::begin()
{
  return option_names_iteratort(&command_line, 0);
}

cmdlinet::option_namest::option_names_iteratort cmdlinet::option_namest::end()
{
  return option_names_iteratort(&command_line, command_line.options.size());
}
