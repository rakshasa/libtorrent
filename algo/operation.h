// rTorrent - BitTorrent client
// Copyright (C) 2005, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef ALGO_OPERATION_H
#define ALGO_OPERATION_H

#include <algo/operation_impl.h>

namespace algo {

ALGO_OPERATION_CODE_ARIT( Add, add, + )
ALGO_OPERATION_CODE_ARTO( AddTo, add_to, add_ref, + )

ALGO_OPERATION_CODE_ARIT( Sub, sub, - )
ALGO_OPERATION_CODE_ARTO( SubTo, sub_to, sub_ref, - )

ALGO_OPERATION_CODE_ARIT( Mul, mul, * )
ALGO_OPERATION_CODE_ARTO( MulTo, mul_to, mul_ref, * )

ALGO_OPERATION_CODE_ARIT( Div, div, / )
ALGO_OPERATION_CODE_ARTO( DivTo, div_to, div_ref, / )

ALGO_OPERATION_CODE_ARIT( Mod, mod, % )
ALGO_OPERATION_CODE_ARTO( ModTo, mod_to, mod_ref, % )

ALGO_OPERATION_CODE_ARTO( Assign, assign, assign_ref, )

// *(Ftor), Ftor() OP back_as_ref
// *(Ftor1, Ftor(), Ftor1() OP Ftor2()

// _BOOl's do not use perlocated return type info.
ALGO_OPERATION_CODE_BOOL( Eq,  eq,  == )
ALGO_OPERATION_CODE_BOOL( Neq, neq, != )
ALGO_OPERATION_CODE_BOOL( Geq, geq, >= )
ALGO_OPERATION_CODE_BOOL( Leq, leq, <= )
ALGO_OPERATION_CODE_BOOL( Gt, gt, > )
ALGO_OPERATION_CODE_BOOL( Lt, lt, < )

ALGO_OPERATION_CODE_BOOL( BoolAnd, bool_and, && )

ALGO_OPERATION_CODE_BOOL_UNI( BoolNot, bool_not, ! )

}

#endif // ALGO_OPERATION_H
