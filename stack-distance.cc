/*
 * Copyright (C) 2023  Xiaoyue Chen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "decode.h"
#include "tracereader.h"

#include <algorithm>
#include <argp.h>
#include <cstddef>
#include <iostream>
#include <list>
#include <numeric>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>

const char *argp_program_version = "stack-distance 0.1";
const char *argp_program_bug_address = "<xiaoyue.chen@it.uu.se>";

static char doc[] = "Predict the stack distance for each memory access";

static char args_doc[] = "TRACE";

const struct argp_option option[]
    = { { "simulate", 's', "N", 0, "Simulate N instructions" }, { 0 } };

struct knobs
{
  size_t nsimulate = 10000000;
  char *trace_file = nullptr;
};

static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  auto knbs = (knobs *)state->input;

  switch (key)
    {
    case 's':
      knbs->nsimulate = atoll (arg);
      break;
    case ARGP_KEY_ARG:
      if (state->arg_num >= 1)
        argp_usage (state);
      knbs->trace_file = arg;
      break;
    case ARGP_KEY_END:
      if (state->arg_num < 1)
        argp_usage (state);
      break;
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static struct argp argp = { option, parse_opt, args_doc, doc };

std::vector<unsigned long long>
create_address_trace (const char *trace_file, size_t n_instruction)
{
  auto reader = sd::tracereader{ trace_file };
  auto address_trace = std::vector<unsigned long long>{};

  using namespace std::ranges;
  for_each (views::iota (size_t{ 0 }, n_instruction), [&] (auto) {
    auto ins = reader.read_single_instr ();
    sd::decode (ins, std::back_inserter (address_trace));
  });
  transform (address_trace, begin (address_trace),
             [] (auto address) { return address >> 6; });
  return address_trace;
}

int
main (int argc, char *argv[])
{
  auto knbs = knobs{};
  argp_parse (&argp, argc, argv, 0, 0, &knbs);

  auto address_trace = create_address_trace (knbs.trace_file, knbs.nsimulate);
  auto forward_reuse_distance = std::vector<size_t> (
      address_trace.size (), std::numeric_limits<size_t>::max ());

  using namespace std::ranges;
  for_each (
      views::iota (size_t{ 0 }, address_trace.size ()),
      [&, last_reference = std::unordered_map<unsigned long long, size_t>{}] (
          auto i) mutable {
        auto address = address_trace[i];
        auto found = last_reference.find (address);
        if (found != end (last_reference))
          {
            auto &last = found->second;
            forward_reuse_distance[last] = i - last;
            last = i;
          }
        else
          {
            last_reference[address] = i;
          }
      });

  auto forward_stack_distance = std::vector<size_t> (
      address_trace.size (), std::numeric_limits<size_t>::max ());
  for_each (
      views::iota (size_t{ 0 }, address_trace.size ()),
      [&, stack = std::list<unsigned long long>{},
       last_reference = std::unordered_map<
           unsigned long long,
           std::pair<size_t, std::list<unsigned long long>::iterator> >{}] (
          auto i) mutable {
        auto address = address_trace[i];
        auto found = last_reference.find (address);
        if (found != end (last_reference))
          {
            auto &[last_i, stack_it] = found->second;
            forward_stack_distance[last_i] = distance (stack_it, end (stack));
            stack.splice (end (stack), stack, stack_it);
            last_i = i;
          }
        else
          {
            auto it = stack.insert (end (stack), address);
            last_reference[address] = std::make_pair (i, it);
          }
      });

  auto forward_stack_distance_est = std::vector<size_t> (
      address_trace.size (), std::numeric_limits<size_t>::max ());
  constexpr static size_t MAX_REUSE_DISTANCE = 64;
  for_each (
      views::iota (size_t{ 0 }, address_trace.size ()),
      [&, scheduler = std::vector<bool> (MAX_REUSE_DISTANCE, false),
       t = size_t{ 0 },
       last_reference
       = std::unordered_map<unsigned long long,
                            std::pair<size_t, size_t> >{}] (auto i) mutable {
        t = t + 1 - scheduler[i % MAX_REUSE_DISTANCE];

        scheduler[i % MAX_REUSE_DISTANCE] = false;

        auto dist = forward_reuse_distance[i];
        auto address = address_trace[i];

        if (dist < MAX_REUSE_DISTANCE)
          {
            scheduler[(i + dist) % MAX_REUSE_DISTANCE] = true;
          }

        auto found = last_reference.find (address);
        if (found != end (last_reference))
          {
            auto &[last_i, last_t] = found->second;
            forward_stack_distance_est[last_i] = t - last_t;
            last_i = i;
            last_t = t;
          }
        else
          {
            last_reference[address] = std::make_pair (i, t);
          }
      });

  printf ("address reuse-distance stack-distance stack-distance-est\n");
  for_each (views::iota (size_t{ 0 }, address_trace.size ()), [&] (auto i) {
    printf ("%llx %zu %zu %zu\n", address_trace[i], forward_reuse_distance[i],
            forward_stack_distance[i], forward_stack_distance_est[i]);
  });

  return 0;
}
