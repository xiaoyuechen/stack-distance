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

#ifndef DECODER_H
#define DECODER_H

#include "trace-instruction.h"

#include <algorithm>
#include <ranges>

namespace sd
{

void
decode (const input_instr &ins, auto back_inserter)
{
  using namespace std::ranges;

  copy (ins.source_memory | views::filter (std::identity{}), back_inserter);
  copy (ins.destination_memory | views::filter (std::identity{}),
        back_inserter);
}

}

#endif
