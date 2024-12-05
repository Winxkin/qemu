/* shake256.c

   The SHAKE256 hash function, arbitrary length output.

   Copyright (C) 2017 Daiki Ueno
   Copyright (C) 2017 Red Hat, Inc.

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see http://www.gnu.org/licenses/.
*/



#include "hw/vst/nettle/sha3.h"
#include "hw/vst/nettle/sha3-internal.h"

void
sha3_256_shake (struct sha3_256_ctx *ctx,
		size_t length, uint8_t *dst)
{
  _nettle_sha3_shake (&ctx->state, sizeof (ctx->block), ctx->block, ctx->index, length, dst);
  sha3_256_init (ctx);
}

void
sha3_256_shake_output (struct sha3_256_ctx *ctx,
		       size_t length, uint8_t *digest)
{
  ctx->index =
    _nettle_sha3_shake_output (&ctx->state,
			       sizeof (ctx->block), ctx->block, ctx->index,
			       length, digest);
}