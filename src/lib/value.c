/*
 * value.c	Functions to handle value_box_t
 *
 * Version:	$Id$
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * Copyright 2014 The FreeRADIUS server project
 */

RCSID("$Id$")

#include <freeradius-devel/libradius.h>
#include <ctype.h>

/** How many bytes wide each of the value data fields are
 *
 * This is useful when copying a value from a value_box_t to a memory
 * location passed as a void *.
 */
size_t const value_box_field_sizes[] = {
	[PW_TYPE_STRING]			= SIZEOF_MEMBER(value_box_t, datum.strvalue),
	[PW_TYPE_OCTETS]			= SIZEOF_MEMBER(value_box_t, datum.octets),

	[PW_TYPE_IPV4_ADDR]			= SIZEOF_MEMBER(value_box_t, datum.ipaddr),
	[PW_TYPE_IPV4_PREFIX]			= SIZEOF_MEMBER(value_box_t, datum.ipv4prefix),
	[PW_TYPE_IPV6_ADDR]			= SIZEOF_MEMBER(value_box_t, datum.ipv6addr),
	[PW_TYPE_IPV6_PREFIX]			= SIZEOF_MEMBER(value_box_t, datum.ipv6prefix),
	[PW_TYPE_IFID]				= SIZEOF_MEMBER(value_box_t, datum.ifid),
	[PW_TYPE_ETHERNET]			= SIZEOF_MEMBER(value_box_t, datum.ether),

	[PW_TYPE_BOOLEAN]			= SIZEOF_MEMBER(value_box_t, datum.boolean),
	[PW_TYPE_BYTE]				= SIZEOF_MEMBER(value_box_t, datum.byte),
	[PW_TYPE_SHORT]				= SIZEOF_MEMBER(value_box_t, datum.ushort),
	[PW_TYPE_INTEGER]			= SIZEOF_MEMBER(value_box_t, datum.integer),
	[PW_TYPE_INTEGER64]			= SIZEOF_MEMBER(value_box_t, datum.integer64),
	[PW_TYPE_SIZE]				= SIZEOF_MEMBER(value_box_t, datum.size),

	[PW_TYPE_SIGNED]			= SIZEOF_MEMBER(value_box_t, datum.sinteger),

	[PW_TYPE_TIMEVAL]			= SIZEOF_MEMBER(value_box_t, datum.timeval),
	[PW_TYPE_DECIMAL]			= SIZEOF_MEMBER(value_box_t, datum.decimal),
	[PW_TYPE_DATE]				= SIZEOF_MEMBER(value_box_t, datum.date),

	[PW_TYPE_ABINARY]			= SIZEOF_MEMBER(value_box_t, datum.filter),
	[PW_TYPE_MAX]				= 0	/* Force compiler to allocate memory for all types */
};

/** Just in case we have a particularly rebellious compiler
 *
 * @note Not even sure if this is required though it does make the code
 * 	more robust in the case where someone changes the order of the
 *	fields in the #value_box_t struct (and we add fields outside of the union).
 */
size_t const value_box_offsets[] = {
	[PW_TYPE_STRING]			= offsetof(value_box_t, datum.strvalue),
	[PW_TYPE_OCTETS]			= offsetof(value_box_t, datum.octets),

	[PW_TYPE_IPV4_ADDR]			= offsetof(value_box_t, datum.ipaddr),
	[PW_TYPE_IPV4_PREFIX]			= offsetof(value_box_t, datum.ipv4prefix),
	[PW_TYPE_IPV6_ADDR]			= offsetof(value_box_t, datum.ipv6addr),
	[PW_TYPE_IPV6_PREFIX]			= offsetof(value_box_t, datum.ipv6prefix),
	[PW_TYPE_IFID]				= offsetof(value_box_t, datum.ifid),
	[PW_TYPE_ETHERNET]			= offsetof(value_box_t, datum.ether),

	[PW_TYPE_BOOLEAN]			= offsetof(value_box_t, datum.boolean),
	[PW_TYPE_BYTE]				= offsetof(value_box_t, datum.byte),
	[PW_TYPE_SHORT]				= offsetof(value_box_t, datum.ushort),
	[PW_TYPE_INTEGER]			= offsetof(value_box_t, datum.integer),
	[PW_TYPE_INTEGER64]			= offsetof(value_box_t, datum.integer64),
	[PW_TYPE_SIZE]				= offsetof(value_box_t, datum.size),

	[PW_TYPE_SIGNED]			= offsetof(value_box_t, datum.sinteger),

	[PW_TYPE_TIMEVAL]			= offsetof(value_box_t, datum.timeval),
	[PW_TYPE_DECIMAL]			= offsetof(value_box_t, datum.decimal),
	[PW_TYPE_DATE]				= offsetof(value_box_t, datum.date),

	[PW_TYPE_ABINARY]			= offsetof(value_box_t, datum.filter),
	[PW_TYPE_MAX]				= 0	/* Force compiler to allocate memory for all types */
};

/** Copy flags and type data from one value box to another
 *
 * @param[in] dst to copy flags to
 * @param[in] src of data.
 */
static inline void value_box_copy_attrs(value_box_t *dst, value_box_t const *src)
{
	dst->type = src->type;
	dst->length = src->length;
	dst->tainted = src->tainted;
	if (fr_dict_enum_types[dst->type]) dst->datum.enumv = src->datum.enumv;
}

/** Compare two values
 *
 * @param[in] a Value to compare.
 * @param[in] b Value to compare.
 * @return
 *	- -1 if a is less than b.
 *	- 0 if both are equal.
 *	- 1 if a is more than b.
 *	- < -1 on failure.
 */
int value_box_cmp(value_box_t const *a, value_box_t const *b)
{
	int compare = 0;

	if (!fr_cond_assert(a->type != PW_TYPE_INVALID)) return -1;
	if (!fr_cond_assert(b->type != PW_TYPE_INVALID)) return -1;

	if (a->type != b->type) {
		fr_strerror_printf("Can't compare values of different types");
		return -2;
	}

	/*
	 *	After doing the previous check for special comparisons,
	 *	do the per-type comparison here.
	 */
	switch (a->type) {
	case PW_TYPE_ABINARY:
	case PW_TYPE_OCTETS:
	case PW_TYPE_STRING:	/* We use memcmp to be \0 safe */
	{
		size_t length;

		if (a->length < b->length) {
			length = a->length;
		} else {
			length = b->length;
		}

		if (length) {
			compare = memcmp(a->datum.octets, b->datum.octets, length);
			if (compare != 0) break;
		}

		/*
		 *	Contents are the same.  The return code
		 *	is therefore the difference in lengths.
		 *
		 *	i.e. "0x00" is smaller than "0x0000"
		 */
		compare = a->length - b->length;
	}
		break;

		/*
		 *	Short-hand for simplicity.
		 */
#define CHECK(_type) if (a->datum._type < b->datum._type)   { compare = -1; \
		} else if (a->datum._type > b->datum._type) { compare = +1; }

	case PW_TYPE_BOOLEAN:	/* this isn't a RADIUS type, and shouldn't really ever be used */
	case PW_TYPE_BYTE:
		CHECK(byte);
		break;

	case PW_TYPE_SHORT:
		CHECK(ushort);
		break;

	case PW_TYPE_DATE:
		CHECK(date);
		break;

	case PW_TYPE_INTEGER:
		CHECK(integer);
		break;

	case PW_TYPE_SIGNED:
		CHECK(sinteger);
		break;

	case PW_TYPE_INTEGER64:
		CHECK(integer64);
		break;

	case PW_TYPE_SIZE:
		CHECK(size);
		break;

	case PW_TYPE_TIMEVAL:
		compare = fr_timeval_cmp(&a->datum.timeval, &b->datum.timeval);
		break;

	case PW_TYPE_DECIMAL:
		CHECK(decimal);
		break;

	case PW_TYPE_ETHERNET:
		compare = memcmp(a->datum.ether, b->datum.ether, sizeof(a->datum.ether));
		break;

	case PW_TYPE_IPV4_ADDR:
	{
		uint32_t a_int, b_int;

		a_int = ntohl(a->datum.ipaddr.s_addr);
		b_int = ntohl(b->datum.ipaddr.s_addr);
		if (a_int < b_int) {
			compare = -1;
		} else if (a_int > b_int) {
			compare = +1;
		}
	}
		break;

	case PW_TYPE_IPV6_ADDR:
		compare = memcmp(&a->datum.ipv6addr, &b->datum.ipv6addr, sizeof(a->datum.ipv6addr));
		break;

	case PW_TYPE_IPV6_PREFIX:
		compare = memcmp(a->datum.ipv6prefix, b->datum.ipv6prefix, sizeof(a->datum.ipv6prefix));
		break;

	case PW_TYPE_IPV4_PREFIX:
		compare = memcmp(a->datum.ipv4prefix, b->datum.ipv4prefix, sizeof(a->datum.ipv4prefix));
		break;

	case PW_TYPE_IFID:
		compare = memcmp(a->datum.ifid, b->datum.ifid, sizeof(a->datum.ifid));
		break;

	/*
	 *	These should be handled at some point
	 */
	case PW_TYPE_COMBO_IP_ADDR:		/* This should have been converted into IPADDR/IPV6ADDR */
	case PW_TYPE_COMBO_IP_PREFIX:		/* This should have been converted into IPADDR/IPV6ADDR */
	case PW_TYPE_STRUCTURAL:
	case PW_TYPE_BAD:
		(void)fr_cond_assert(0);	/* unknown type */
		return -2;

	/*
	 *	Do NOT add a default here, as new types are added
	 *	static analysis will warn us they're not handled
	 */
	}

	if (compare > 0) return 1;
	if (compare < 0) return -1;
	return 0;
}

/*
 *	We leverage the fact that IPv4 and IPv6 prefixes both
 *	have the same format:
 *
 *	reserved, prefix-len, data...
 */
static int value_box_cidr_cmp_op(FR_TOKEN op, int bytes,
				  uint8_t a_net, uint8_t const *a,
				  uint8_t b_net, uint8_t const *b)
{
	int i, common;
	uint32_t mask;

	/*
	 *	Handle the case of netmasks being identical.
	 */
	if (a_net == b_net) {
		int compare;

		compare = memcmp(a, b, bytes);

		/*
		 *	If they're identical return true for
		 *	identical.
		 */
		if ((compare == 0) &&
		    ((op == T_OP_CMP_EQ) ||
		     (op == T_OP_LE) ||
		     (op == T_OP_GE))) {
			return true;
		}

		/*
		 *	Everything else returns false.
		 *
		 *	10/8 == 24/8  --> false
		 *	10/8 <= 24/8  --> false
		 *	10/8 >= 24/8  --> false
		 */
		return false;
	}

	/*
	 *	Netmasks are different.  That limits the
	 *	possible results, based on the operator.
	 */
	switch (op) {
	case T_OP_CMP_EQ:
		return false;

	case T_OP_NE:
		return true;

	case T_OP_LE:
	case T_OP_LT:	/* 192/8 < 192.168/16 --> false */
		if (a_net < b_net) {
			return false;
		}
		break;

	case T_OP_GE:
	case T_OP_GT:	/* 192/16 > 192.168/8 --> false */
		if (a_net > b_net) {
			return false;
		}
		break;

	default:
		return false;
	}

	if (a_net < b_net) {
		common = a_net;
	} else {
		common = b_net;
	}

	/*
	 *	Do the check byte by byte.  If the bytes are
	 *	identical, it MAY be a match.  If they're different,
	 *	it is NOT a match.
	 */
	i = 0;
	while (i < bytes) {
		/*
		 *	All leading bytes are identical.
		 */
		if (common == 0) return true;

		/*
		 *	Doing bitmasks takes more work.
		 */
		if (common < 8) break;

		if (a[i] != b[i]) return false;

		common -= 8;
		i++;
		continue;
	}

	mask = 1;
	mask <<= (8 - common);
	mask--;
	mask = ~mask;

	if ((a[i] & mask) == ((b[i] & mask))) {
		return true;
	}

	return false;
}

/** Compare two attributes using an operator
 *
 * @param[in] op to use in comparison.
 * @param[in] a Value to compare.
 * @param[in] b Value to compare.
 * @return
 *	- 1 if true
 *	- 0 if false
 *	- -1 on failure.
 */
int value_box_cmp_op(FR_TOKEN op, value_box_t const *a, value_box_t const *b)
{
	int compare = 0;

	if (!a || !b) return -1;

	if (!fr_cond_assert(a->type != PW_TYPE_INVALID)) return -1;
	if (!fr_cond_assert(b->type != PW_TYPE_INVALID)) return -1;

	switch (a->type) {
	case PW_TYPE_IPV4_ADDR:
		switch (b->type) {
		case PW_TYPE_IPV4_ADDR:		/* IPv4 and IPv4 */
			goto cmp;

		case PW_TYPE_IPV4_PREFIX:	/* IPv4 and IPv4 Prefix */
			return value_box_cidr_cmp_op(op, 4, 32, (uint8_t const *) &a->datum.ipaddr,
						     b->datum.ipv4prefix[1], (uint8_t const *) &b->datum.ipv4prefix[2]);

		default:
			fr_strerror_printf("Cannot compare IPv4 with IPv6 address");
			return -1;
		}

	case PW_TYPE_IPV4_PREFIX:		/* IPv4 and IPv4 Prefix */
		switch (b->type) {
		case PW_TYPE_IPV4_ADDR:
			return value_box_cidr_cmp_op(op, 4, a->datum.ipv4prefix[1],
						     (uint8_t const *) &a->datum.ipv4prefix[2],
						     32, (uint8_t const *) &b->datum.ipaddr);

		case PW_TYPE_IPV4_PREFIX:	/* IPv4 Prefix and IPv4 Prefix */
			return value_box_cidr_cmp_op(op, 4, a->datum.ipv4prefix[1],
						     (uint8_t const *) &a->datum.ipv4prefix[2],
						     b->datum.ipv4prefix[1], (uint8_t const *) &b->datum.ipv4prefix[2]);

		default:
			fr_strerror_printf("Cannot compare IPv4 with IPv6 address");
			return -1;
		}

	case PW_TYPE_IPV6_ADDR:
		switch (b->type) {
		case PW_TYPE_IPV6_ADDR:		/* IPv6 and IPv6 */
			goto cmp;

		case PW_TYPE_IPV6_PREFIX:	/* IPv6 and IPv6 Preifx */
			return value_box_cidr_cmp_op(op, 16, 128, (uint8_t const *) &a->datum.ipv6addr,
						     b->datum.ipv6prefix[1], (uint8_t const *) &b->datum.ipv6prefix[2]);

		default:
			fr_strerror_printf("Cannot compare IPv6 with IPv4 address");
			return -1;
		}

	case PW_TYPE_IPV6_PREFIX:
		switch (b->type) {
		case PW_TYPE_IPV6_ADDR:		/* IPv6 Prefix and IPv6 */
			return value_box_cidr_cmp_op(op, 16, a->datum.ipv6prefix[1],
						     (uint8_t const *) &a->datum.ipv6prefix[2],
						     128, (uint8_t const *) &b->datum.ipv6addr);

		case PW_TYPE_IPV6_PREFIX:	/* IPv6 Prefix and IPv6 */
			return value_box_cidr_cmp_op(op, 16, a->datum.ipv6prefix[1],
						     (uint8_t const *) &a->datum.ipv6prefix[2],
						     b->datum.ipv6prefix[1], (uint8_t const *) &b->datum.ipv6prefix[2]);

		default:
			fr_strerror_printf("Cannot compare IPv6 with IPv4 address");
			return -1;
		}

	default:
	cmp:
		compare = value_box_cmp(a, b);
		if (compare < -1) {	/* comparison error */
			return -1;
		}
	}

	/*
	 *	Now do the operator comparison.
	 */
	switch (op) {
	case T_OP_CMP_EQ:
		return (compare == 0);

	case T_OP_NE:
		return (compare != 0);

	case T_OP_LT:
		return (compare < 0);

	case T_OP_GT:
		return (compare > 0);

	case T_OP_LE:
		return (compare <= 0);

	case T_OP_GE:
		return (compare >= 0);

	default:
		return 0;
	}
}

/** Match all fixed length types in case statements
 *
 * @note This should be used for switch statements in printing and casting
 *	functions that need to deal with all types representing values
 */
#define PW_TYPE_BOUNDED \
	     PW_TYPE_BYTE: \
	case PW_TYPE_SHORT: \
	case PW_TYPE_INTEGER: \
	case PW_TYPE_INTEGER64: \
	case PW_TYPE_SIZE: \
	case PW_TYPE_DATE: \
	case PW_TYPE_IFID: \
	case PW_TYPE_ETHERNET: \
	case PW_TYPE_COMBO_IP_ADDR: \
	case PW_TYPE_COMBO_IP_PREFIX: \
	case PW_TYPE_SIGNED: \
	case PW_TYPE_TIMEVAL: \
	case PW_TYPE_BOOLEAN: \
	case PW_TYPE_DECIMAL

/** Match all variable length types in case statements
 *
 * @note This should be used for switch statements in printing and casting
 *	functions that need to deal with all types representing values
 */
#define PW_TYPE_UNBOUNDED \
	     PW_TYPE_STRING: \
	case PW_TYPE_OCTETS: \
	case PW_TYPE_ABINARY: \
	case PW_TYPE_IPV4_ADDR: \
	case PW_TYPE_IPV4_PREFIX: \
	case PW_TYPE_IPV6_ADDR: \
	case PW_TYPE_IPV6_PREFIX

static char const hextab[] = "0123456789abcdef";

/** Convert a string value with escape sequences into its binary form
 *
 * The quote character determines the escape sequences recognised.
 *
 * Literal mode ("'" quote char) will unescape:
 @verbatim
   - \\        - Literal backslash.
   - \<quote>  - The quotation char.
 @endverbatim
 *
 * Expanded mode (any other quote char) will also unescape:
 @verbatim
   - \r        - Carriage return.
   - \n        - Newline.
   - \t        - Tab.
   - \<oct>    - An octal escape sequence.
   - \x<hex>   - A hex escape sequence.
 @endverbatim
 *
 * Verbatim mode ("\0") passing \0 as the quote char copies in to out verbatim.
 *
 * @note The resulting string will not be \0 terminated, and may contain embedded \0s.
 * @note Invalid escape sequences will be copied verbatim.
 * @note in and out may point to the same buffer.
 *
 * @param[out] out	Where to write the unescaped string.
 *			Unescaping never introduces additional chars.
 * @param[in] in	The string to unescape.
 * @param[in] inlen	Length of input string.
 * @param[in] quote	Character around the string, determines unescaping mode.
 *
 * @return >= 0 the number of bytes written to out.
 */
size_t fr_value_str_unescape(uint8_t *out, char const *in, size_t inlen, char quote)
{
	char const	*p = in;
	uint8_t		*out_p = out;
	int		x;

	/*
	 *	No de-quoting.  Just copy the string.
	 */
	if (!quote) {
		memcpy(out, in, inlen);
		return inlen;
	}

	/*
	 *	Do escaping for single quoted strings.  Only
	 *	single quotes get escaped.  Everything else is
	 *	left as-is.
	 */
	if (quote == '\'') {
		while (p < (in + inlen)) {
			/*
			 *	The quotation character is escaped.
			 */
			if ((p[0] == '\\') &&
			    (p[1] == quote)) {
				*(out_p++) = quote;
				p += 2;
				continue;
			}

			/*
			 *	Two backslashes get mangled to one.
			 */
			if ((p[0] == '\\') &&
			    (p[1] == '\\')) {
				*(out_p++) = '\\';
				p += 2;
				continue;
			}

			/*
			 *	Not escaped, just copy it over.
			 */
			*(out_p++) = *(p++);
		}
		return out_p - out;
	}

	/*
	 *	It's "string" or `string`, do all standard
	 *	escaping.
	 */
	while (p < (in + inlen)) {
		uint8_t c = *p++;
		uint8_t *h0, *h1;

		/*
		 *	We copy all invalid escape sequences verbatim,
		 *	even if they occur at the end of sthe string.
		 */
		if ((c == '\\') && (p >= (in + inlen))) {
		invalid_escape:
			*out_p++ = c;
			while (p < (in + inlen)) *out_p++ = *p++;
			return out_p - out;
		}

		/*
		 *	Fix up \[rnt\\] -> ... the binary form of it.
		 */
		if (c == '\\') {
			switch (*p) {
			case 'r':
				c = '\r';
				p++;
				break;

			case 'n':
				c = '\n';
				p++;
				break;

			case 't':
				c = '\t';
				p++;
				break;

			case '\\':
				c = '\\';
				p++;
				break;

			default:
				/*
				 *	\" --> ", but only inside of double quoted strings, etc.
				 */
				if (*p == quote) {
					c = quote;
					p++;
					break;
				}

				/*
				 *	We need at least three chars, for either octal or hex
				 */
				if ((p + 2) >= (in + inlen)) goto invalid_escape;

				/*
				 *	\x00 --> binary zero character
				 */
				if ((p[0] == 'x') &&
				    (h0 = memchr((uint8_t const *)hextab, tolower((int) p[1]), sizeof(hextab))) &&
				    (h1 = memchr((uint8_t const *)hextab, tolower((int) p[2]), sizeof(hextab)))) {
				 	c = ((h0 - (uint8_t const *)hextab) << 4) + (h1 - (uint8_t const *)hextab);
				 	p += 3;
				}

				/*
				 *	\000 --> binary zero character
				 */
				if ((p[0] >= '0') &&
				    (p[0] <= '9') &&
				    (p[1] >= '0') &&
				    (p[1] <= '9') &&
				    (p[2] >= '0') &&
				    (p[2] <= '9') &&
				    (sscanf(p, "%3o", &x) == 1)) {
					c = x;
					p += 3;
				}

				/*
				 *	Else It's not a recognised escape sequence DON'T
				 *	consume the backslash. This is identical
				 *	behaviour to bash and most other things that
				 *	use backslash escaping.
				 */
			}
		}
		*out_p++ = c;
	}

	return out_p - out;
}

/** Clear/free any existing value
 *
 * @note Do not use on uninitialised memory.
 *
 * @param[in] data to clear.
 */
void value_box_clear(value_box_t *data)
{
	switch (data->type) {
	case PW_TYPE_OCTETS:
	case PW_TYPE_STRING:
		TALLOC_FREE(data->datum.ptr);
		break;

	case PW_TYPE_STRUCTURAL:
		if (!fr_cond_assert(0)) return;

	case PW_TYPE_INVALID:
		return;

	default:
		memset(&data->datum, 0, dict_attr_sizes[data->type][1]);
		break;
	}

	data->tainted = false;
	data->type = PW_TYPE_INVALID;
	data->length = 0;
}

/** Convert string value to a value_box_t type
 *
 * @todo Should take taint param.
 *
 * @param[in] ctx		to alloc strings in.
 * @param[out] dst		where to write parsed value.
 * @param[in,out] dst_type	of value data to create/dst_type of value created.
 * @param[in] dst_enumv		fr_dict_attr_t with string aliases for integer values.
 * @param[in] in		String to convert. Binary safe for variable length values
 *				if len is provided.
 * @param[in] inlen		may be < 0 in which case strlen(len) is used to determine
 *				length, else inlen should be the length of the string or
 *				sub string to parse.
 * @param[in] quote		character used set unescape mode.  @see fr_value_str_unescape.
 * @return
 *	- 0 on success.
 *	- -1 on parse error.
 */
int value_box_from_str(TALLOC_CTX *ctx, value_box_t *dst,
		       PW_TYPE *dst_type, fr_dict_attr_t const *dst_enumv,
		       char const *in, ssize_t inlen, char quote)
{
	fr_dict_enum_t	*dval;
	size_t		len;
	ssize_t		ret;
	char		buffer[256];

	if (!fr_cond_assert(*dst_type != PW_TYPE_INVALID)) return -1;

	if (!in) return -1;

	len = (inlen < 0) ? strlen(in) : (size_t)inlen;

	/*
	 *	Set size for all fixed length attributes.
	 */
	ret = dict_attr_sizes[*dst_type][1];	/* Max length */

	/*
	 *	It's a variable ret src->dst_type so we just alloc a new buffer
	 *	of size len and copy.
	 */
	switch (*dst_type) {
	case PW_TYPE_STRING:
	{
		char *buff, *p;

		buff = talloc_bstrndup(ctx, in, len);

		/*
		 *	No de-quoting.  Just copy the string.
		 */
		if (!quote) {
			ret = len;
			dst->datum.strvalue = buff;
			goto finish;
		}

		len = fr_value_str_unescape((uint8_t *)buff, in, len, quote);

		/*
		 *	Shrink the buffer to the correct size
		 *	and \0 terminate it.  There is a significant
		 *	amount of legacy code that assumes the string
		 *	buffer in value pairs is a C string.
		 *
		 *	It's better for the server to print partial
		 *	strings, instead of SEGV.
		 */
		dst->datum.strvalue = p = talloc_realloc(ctx, buff, char, len + 1);
		p[len] = '\0';
		ret = len;
	}
		goto finish;

	case PW_TYPE_VSA:
		fr_strerror_printf("Must use 'Attr-26 = ...' instead of 'Vendor-Specific = ...'");
		return -1;

	/* raw octets: 0x01020304... */
	case PW_TYPE_OCTETS:
	{
		uint8_t	*p;

		/*
		 *	No 0x prefix, just copy verbatim.
		 */
		if ((len < 2) || (strncasecmp(in, "0x", 2) != 0)) {
			dst->datum.octets = talloc_memdup(ctx, (uint8_t const *)in, len);
			talloc_set_type(dst->datum.octets, uint8_t);
			ret = len;
			goto finish;
		}

		len -= 2;

		/*
		 *	Invalid.
		 */
		if ((len & 0x01) != 0) {
			fr_strerror_printf("Length of Hex String is not even, got %zu bytes", len);
			return -1;
		}

		ret = len >> 1;
		p = talloc_array(ctx, uint8_t, ret);
		if (fr_hex2bin(p, ret, in + 2, len) != (size_t)ret) {
			talloc_free(p);
			fr_strerror_printf("Invalid hex data");
			return -1;
		}

		dst->datum.octets = p;
	}
		goto finish;

	case PW_TYPE_ABINARY:
#ifdef WITH_ASCEND_BINARY
		if ((len > 1) && (strncasecmp(in, "0x", 2) == 0)) {
			ssize_t bin;

			if (len > ((sizeof(dst->datum.filter) + 1) * 2)) {
				fr_strerror_printf("Hex data is too large for ascend filter");
				return -1;
			}

			bin = fr_hex2bin((uint8_t *) &dst->datum.filter, ret, in + 2, len - 2);
			if (bin < ret) {
				memset(((uint8_t *) &dst->datum.filter) + bin, 0, ret - bin);
			}
		} else {
			if (ascend_parse_filter(dst, in, len) < 0 ) {
				/* Allow ascend_parse_filter's strerror to bubble up */
				return -1;
			}
		}

		ret = sizeof(dst->datum.filter);
		goto finish;
#else
		/*
		 *	If Ascend binary is NOT defined,
		 *	then fall through to raw octets, so that
		 *	the user can at least make them by hand...
		 */
	 	goto do_octets;
#endif

	case PW_TYPE_IPV4_ADDR:
	{
		fr_ipaddr_t addr;

		if (fr_inet_pton4(&addr, in, inlen, fr_hostname_lookups, false, true) < 0) return -1;

		/*
		 *	We allow v4 addresses to have a /32 suffix as some databases (PostgreSQL)
		 *	print them this way.
		 */
		if (addr.prefix != 32) {
			fr_strerror_printf("Invalid IPv4 mask length \"/%i\".  Only \"/32\" permitted "
					   "for non-prefix types", addr.prefix);
			return -1;
		}

		dst->datum.ipaddr.s_addr = addr.ipaddr.ip4addr.s_addr;
	}
		goto finish;

	case PW_TYPE_IPV4_PREFIX:
	{
		fr_ipaddr_t addr;

		if (fr_inet_pton4(&addr, in, inlen, fr_hostname_lookups, false, true) < 0) return -1;

		dst->datum.ipv4prefix[1] = addr.prefix;
		memcpy(&dst->datum.ipv4prefix[2], &addr.ipaddr.ip4addr.s_addr, sizeof(dst->datum.ipv4prefix) - 2);
	}
		goto finish;

	case PW_TYPE_IPV6_ADDR:
	{
		fr_ipaddr_t addr;

		if (fr_inet_pton6(&addr, in, inlen, fr_hostname_lookups, false, true) < 0) return -1;

		/*
		 *	We allow v6 addresses to have a /128 suffix as some databases (PostgreSQL)
		 *	print them this way.
		 */
		if (addr.prefix != 128) {
			fr_strerror_printf("Invalid IPv6 mask length \"/%i\".  Only \"/128\" permitted "
					   "for non-prefix types", addr.prefix);
			return -1;
		}

		memcpy(&dst->datum.ipv6addr, addr.ipaddr.ip6addr.s6_addr, sizeof(dst->datum.ipv6addr));
	}
		goto finish;

	case PW_TYPE_IPV6_PREFIX:
	{
		fr_ipaddr_t addr;

		if (fr_inet_pton6(&addr, in, inlen, fr_hostname_lookups, false, true) < 0) return -1;

		dst->datum.ipv6prefix[1] = addr.prefix;
		memcpy(&dst->datum.ipv6prefix[2], addr.ipaddr.ip6addr.s6_addr, sizeof(dst->datum.ipv6prefix) - 2);
	}
		goto finish;

	/*
	 *	Dealt with below
	 */
	case PW_TYPE_BOUNDED:
		break;

	case PW_TYPE_STRUCTURAL_EXCEPT_VSA:
	case PW_TYPE_VENDOR:
	case PW_TYPE_BAD:
		fr_strerror_printf("Invalid dst_type %d", *dst_type);
		return -1;
	}

	/*
	 *	It's a fixed size src->dst_type, copy to a temporary buffer and
	 *	\0 terminate if insize >= 0.
	 */
	if (inlen > 0) {
		if (len >= sizeof(buffer)) {
			fr_strerror_printf("Temporary buffer too small");
			return -1;
		}

		memcpy(buffer, in, inlen);
		buffer[inlen] = '\0';
		in = buffer;
	}

	switch (*dst_type) {
	case PW_TYPE_BYTE:
	{
		char *p;
		unsigned int i;

		/*
		 *	Note that ALL integers are unsigned!
		 */
		i = fr_strtoul(in, &p);

		/*
		 *	Look for the named in for the given
		 *	attribute.
		 */
		if (dst_enumv && *p && !is_whitespace(p)) {
			if ((dval = fr_dict_enum_by_name(dst_enumv, in)) == NULL) {
				fr_strerror_printf("Unknown or invalid value \"%s\" for attribute %s",
						   in, dst_enumv->name);
				return -1;
			}

			dst->datum.byte = dval->value;
		} else {
			if (i > 255) {
				fr_strerror_printf("Byte value \"%s\" is larger than 255", in);
				return -1;
			}

			dst->datum.byte = i;
		}
		break;
	}

	case PW_TYPE_SHORT:
	{
		char *p;
		unsigned int i;

		/*
		 *	Note that ALL integers are unsigned!
		 */
		i = fr_strtoul(in, &p);

		/*
		 *	Look for the named in for the given
		 *	attribute.
		 */
		if (dst_enumv && *p && !is_whitespace(p)) {
			if ((dval = fr_dict_enum_by_name(dst_enumv, in)) == NULL) {
				fr_strerror_printf("Unknown or invalid value \"%s\" for attribute %s",
						   in, dst_enumv->name);
				return -1;
			}

			dst->datum.ushort = dval->value;
		} else {
			if (i > 65535) {
				fr_strerror_printf("Short value \"%s\" is larger than 65535", in);
				return -1;
			}

			dst->datum.ushort = i;
		}
		break;
	}

	case PW_TYPE_INTEGER:
	{
		char *p;
		unsigned int i;

		/*
		 *	Note that ALL integers are unsigned!
		 */
		i = fr_strtoul(in, &p);

		/*
		 *	Look for the named in for the given
		 *	attribute.
		 */
		if (dst_enumv && *p && !is_whitespace(p)) {
			if ((dval = fr_dict_enum_by_name(dst_enumv, in)) == NULL) {
				fr_strerror_printf("Unknown or invalid value \"%s\" for attribute %s",
						   in, dst_enumv->name);
				return -1;
			}

			dst->datum.integer = dval->value;
		} else {
			/*
			 *	Value is always within the limits
			 */
			dst->datum.integer = i;
		}
	}
		break;

	case PW_TYPE_INTEGER64:
	{
		uint64_t i;

		/*
		 *	Note that ALL integers are unsigned!
		 */
		if (sscanf(in, "%" PRIu64, &i) != 1) {
			fr_strerror_printf("Failed parsing \"%s\" as unsigned 64bit integer", in);
			return -1;
		}
		dst->datum.integer64 = i;
	}
		break;

	case PW_TYPE_SIZE:
	{
		size_t i;

		if (sscanf(in, "%zu", &i) != 1) {
			fr_strerror_printf("Failed parsing \"%s\" as a file or memory size", in);
			return -1;
		}
		dst->datum.size = i;
	}
		break;

	case PW_TYPE_TIMEVAL:
		if (fr_timeval_from_str(&dst->datum.timeval, in) < 0) return -1;
		break;

	case PW_TYPE_DECIMAL:
	{
		double d;

		if (sscanf(in, "%lf", &d) != 1) {
			fr_strerror_printf("Failed parsing \"%s\" as a decimal", in);
			return -1;
		}
		dst->datum.decimal = d;
	}
		break;

	case PW_TYPE_DATE:
	{
		/*
		 *	time_t may be 64 bits, whule vp_date MUST be 32-bits.  We need an
		 *	intermediary variable to handle the conversions.
		 */
		time_t date;

		if (fr_time_from_str(&date, in) < 0) {
			fr_strerror_printf("failed to parse time string \"%s\"", in);
			return -1;
		}

		dst->datum.date = date;
	}

		break;

	case PW_TYPE_IFID:
		if (fr_inet_ifid_pton((void *) dst->datum.ifid, in) == NULL) {
			fr_strerror_printf("Failed to parse interface-id string \"%s\"", in);
			return -1;
		}
		break;

	case PW_TYPE_ETHERNET:
	{
		char const *c1, *c2, *cp;
		size_t p_len = 0;

		/*
		 *	Convert things which are obviously integers to Ethernet addresses
		 *
		 *	We assume the number is the bigendian representation of the
		 *	ethernet address.
		 */
		if (is_integer(in)) {
			uint64_t integer = htonll(atoll(in));

			memcpy(dst->datum.ether, &integer, sizeof(dst->datum.ether));
			break;
		}

		cp = in;
		while (*cp) {
			if (cp[1] == ':') {
				c1 = hextab;
				c2 = memchr(hextab, tolower((int) cp[0]), 16);
				cp += 2;
			} else if ((cp[1] != '\0') && ((cp[2] == ':') || (cp[2] == '\0'))) {
				c1 = memchr(hextab, tolower((int) cp[0]), 16);
				c2 = memchr(hextab, tolower((int) cp[1]), 16);
				cp += 2;
				if (*cp == ':') cp++;
			} else {
				c1 = c2 = NULL;
			}
			if (!c1 || !c2 || (p_len >= sizeof(dst->datum.ether))) {
				fr_strerror_printf("failed to parse Ethernet address \"%s\"", in);
				return -1;
			}
			dst->datum.ether[p_len] = ((c1-hextab)<<4) + (c2-hextab);
			p_len++;
		}
	}
		break;

	/*
	 *	Crazy polymorphic (IPv4/IPv6) attribute src->dst_type for WiMAX.
	 *
	 *	We try and make is saner by replacing the original
	 *	da, with either an IPv4 or IPv6 da src->dst_type.
	 *
	 *	These are not dynamic da, and will have the same vendor
	 *	and attribute as the original.
	 */
	case PW_TYPE_COMBO_IP_ADDR:
	{
		if (inet_pton(AF_INET6, in, &dst->datum.ipv6addr) > 0) {
			*dst_type = PW_TYPE_IPV6_ADDR;
			ret = dict_attr_sizes[PW_TYPE_COMBO_IP_ADDR][1]; /* size of IPv6 address */
		} else {
			fr_ipaddr_t ipaddr;

			if (fr_inet_hton(&ipaddr, AF_INET, in, false) < 0) {
				fr_strerror_printf("Failed to find IPv4 address for %s", in);
				return -1;
			}

			*dst_type = PW_TYPE_IPV4_ADDR;
			dst->datum.ipaddr.s_addr = ipaddr.ipaddr.ip4addr.s_addr;
			ret = dict_attr_sizes[PW_TYPE_COMBO_IP_ADDR][0]; /* size of IPv4 address */
		}
	}
		break;

	case PW_TYPE_SIGNED:
		/* Damned code for 1 WiMAX attribute */
		dst->datum.sinteger = (int32_t)strtol(in, NULL, 10);
		break;

	case PW_TYPE_BOOLEAN:
	case PW_TYPE_COMBO_IP_PREFIX:
		break;

	case PW_TYPE_UNBOUNDED:		/* Should have been dealt with above */
	case PW_TYPE_STRUCTURAL:	/* Listed again to suppress compiler warnings */
	case PW_TYPE_BAD:
		fr_strerror_printf("Unknown attribute dst_type %d", *dst_type);
		return -1;
	}

finish:
	dst->length = ret;
	dst->type = *dst_type;

	/*
	 *	Fixup enumv
	 */
	if (fr_dict_enum_types[dst->type]) dst->datum.enumv = dst_enumv;

	return 0;
}

/** Performs byte order reversal for types that need it
 *
 * @param[in] dst	Where to write the result.  May be the same as src.
 * @param[in] src	#value_box_t containing an integer value.
 * @return
 *	- 0 on success.
 *	- -1 on failure.
 */
int value_box_hton(value_box_t *dst, value_box_t const *src)
{
	if (!fr_cond_assert(src->type != PW_TYPE_INVALID)) return -1;

	/* 8 byte integers */
	switch (src->type) {
	case PW_TYPE_INTEGER64:
		dst->datum.integer64 = htonll(src->datum.integer64);
		break;

	/* 4 byte integers */
	case PW_TYPE_INTEGER:
	case PW_TYPE_DATE:
	case PW_TYPE_SIGNED:
		dst->datum.integer = htonl(src->datum.integer);
		break;

	/* 2 byte integers */
	case PW_TYPE_SHORT:
		dst->datum.ushort = htons(src->datum.ushort);
		break;

	case PW_TYPE_OCTETS:
	case PW_TYPE_STRING:
		if (!fr_cond_assert(0)) return -1; /* shouldn't happen */

	default:
		value_box_copy(NULL, dst, src);
		break;
	}

	value_box_copy_attrs(dst, src);

	return 0;
}

/** Convert one type of value_box_t to another
 *
 * @note This should be the canonical function used to convert between data types.
 *
 * @param ctx to allocate buffers in (usually the same as dst)
 * @param dst Where to write result of casting.
 * @param dst_type to cast to.
 * @param dst_enumv Enumerated values used to converts strings to integers.
 * @param src Input data.
 * @return
 *	- 0 on success.
 *	- -1 on failure.
 */
int value_box_cast(TALLOC_CTX *ctx, value_box_t *dst,
		   PW_TYPE dst_type, fr_dict_attr_t const *dst_enumv,
		   value_box_t const *src)
{
	if (!fr_cond_assert(dst_type != PW_TYPE_INVALID)) return -1;
	if (!fr_cond_assert(src->type != PW_TYPE_INVALID)) return -1;

	if (fr_dict_non_data_types[dst_type]) {
		fr_strerror_printf("Invalid cast from %s to %s.  Can only cast simple data types.",
				   fr_int2str(dict_attr_types, src->type, "<INVALID>"),
				   fr_int2str(dict_attr_types, dst_type, "<INVALID>"));
		return -1;
	}

	/*
	 *	If it's the same type, copy.
	 */
	if (dst_type == src->type) return value_box_copy(ctx, dst, src);

	/*
	 *	Deserialise a value_box_t
	 */
	if (src->type == PW_TYPE_STRING) {
		return value_box_from_str(ctx, dst, &dst_type, dst_enumv, src->datum.strvalue, src->length, '\0');
	}

	/*
	 *	Converts the src data to octets with no processing.
	 */
	if (dst_type == PW_TYPE_OCTETS) {
		value_box_hton(dst, src);
		dst->datum.octets = talloc_memdup(ctx, &dst->datum, src->length);
		dst->length = src->length;
		dst->type = dst_type;
		talloc_set_type(dst->datum.octets, uint8_t);
		return 0;
	}

	/*
	 *	Serialise a value_box_t
	 */
	if (dst_type == PW_TYPE_STRING) {
		dst->datum.strvalue = value_box_asprint(ctx, src, '\0');
		dst->length = talloc_array_length(dst->datum.strvalue) - 1;
		dst->type = dst_type;
		return 0;
	}

	if ((src->type == PW_TYPE_IFID) &&
	    (dst_type == PW_TYPE_INTEGER64)) {
		memcpy(&dst->datum.integer64, src->datum.ifid, sizeof(src->datum.ifid));
		dst->datum.integer64 = htonll(dst->datum.integer64);

	fixed_length:
		dst->length = dict_attr_sizes[dst_type][0];
		dst->type = dst_type;
		if (fr_dict_enum_types[dst_type]) dst->datum.enumv = dst_enumv;

		return 0;
	}

	if ((src->type == PW_TYPE_INTEGER64) &&
	    (dst_type == PW_TYPE_ETHERNET)) {
		uint8_t array[8];
		uint64_t i;

		i = htonll(src->datum.integer64);
		memcpy(array, &i, 8);

		/*
		 *	For OUIs in the DB.
		 */
		if ((array[0] != 0) || (array[1] != 0)) return -1;

		memcpy(dst->datum.ether, &array[2], 6);
		goto fixed_length;
	}

	/*
	 *	For integers, we allow the casting of a SMALL type to
	 *	a larger type, but not vice-versa.
	 */
	if (dst_type == PW_TYPE_SHORT) {
		switch (src->type) {
		case PW_TYPE_BYTE:
			dst->datum.ushort = src->datum.byte;
			break;

		case PW_TYPE_OCTETS:
			goto do_octets;

		default:
			goto invalid_cast;
		}
		goto fixed_length;
	}

	/*
	 *	We can cast LONG integers to SHORTER ones, so long
	 *	as the long one is on the LHS.
	 */
	if (dst_type == PW_TYPE_INTEGER) {
		switch (src->type) {
		case PW_TYPE_BYTE:
			dst->datum.integer = src->datum.byte;
			break;

		case PW_TYPE_SHORT:
			dst->datum.integer = src->datum.ushort;
			break;

		case PW_TYPE_SIGNED:
			if (src->datum.sinteger < 0 ) {
				fr_strerror_printf("Invalid cast: From signed to integer.  signed value %d is negative ",
						    src->datum.sinteger);
				return -1;
			}
			dst->datum.integer = (uint32_t)src->datum.sinteger;
			break;

		case PW_TYPE_OCTETS:
			goto do_octets;

		default:
			goto invalid_cast;
		}
		goto fixed_length;
	}

	/*
	 *	For integers, we allow the casting of a SMALL type to
	 *	a larger type, but not vice-versa.
	 */
	if (dst_type == PW_TYPE_INTEGER64) {
		switch (src->type) {
		case PW_TYPE_BYTE:
			dst->datum.integer64 = src->datum.byte;
			break;

		case PW_TYPE_SHORT:
			dst->datum.integer64 = src->datum.ushort;
			break;

		case PW_TYPE_INTEGER:
			dst->datum.integer64 = src->datum.integer;
			break;

		case PW_TYPE_DATE:
			dst->datum.integer64 = src->datum.date;
			break;

		case PW_TYPE_OCTETS:
			goto do_octets;

		default:
		invalid_cast:
			fr_strerror_printf("Invalid cast from %s to %s",
					   fr_int2str(dict_attr_types, src->type, "<INVALID>"),
					   fr_int2str(dict_attr_types, dst_type, "<INVALID>"));
			return -1;

		}
		goto fixed_length;
	}

	/*
	 *	We can cast integers less that < INT_MAX to signed
	 */
	if (dst_type == PW_TYPE_SIGNED) {
		switch (src->type) {
		case PW_TYPE_BYTE:
			dst->datum.sinteger = src->datum.byte;
			break;

		case PW_TYPE_SHORT:
			dst->datum.sinteger = src->datum.ushort;
			break;

		case PW_TYPE_INTEGER:
			if (src->datum.integer > INT_MAX) {
				fr_strerror_printf("Invalid cast: From integer to signed.  integer value %u is larger "
						   "than max signed int and would overflow", src->datum.integer);
				return -1;
			}
			dst->datum.sinteger = (int)src->datum.integer;
			break;

		case PW_TYPE_INTEGER64:
			if (src->datum.integer > INT_MAX) {
				fr_strerror_printf("Invalid cast: From integer64 to signed.  integer64 value %" PRIu64
						   " is larger than max signed int and would overflow", src->datum.integer64);
				return -1;
			}
			dst->datum.sinteger = (int)src->datum.integer64;
			break;

		case PW_TYPE_OCTETS:
			goto do_octets;

		default:
			goto invalid_cast;
		}
		goto fixed_length;
	}

	if (dst_type == PW_TYPE_TIMEVAL) {
		switch (src->type) {
		case PW_TYPE_BYTE:
			dst->datum.timeval.tv_sec = src->datum.byte;
			dst->datum.timeval.tv_usec = 0;
			break;

		case PW_TYPE_SHORT:
			dst->datum.timeval.tv_sec = src->datum.ushort;
			dst->datum.timeval.tv_usec = 0;
			break;

		case PW_TYPE_INTEGER:
			dst->datum.timeval.tv_sec = src->datum.integer;
			dst->datum.timeval.tv_usec = 0;
			break;

		case PW_TYPE_INTEGER64:
			/*
			 *	tv_sec is a time_t, which is variable in size
			 *	depending on the system.
			 *
			 *	It should be >= 64bits on modern systems,
			 *	but you never know...
			 */
			if (sizeof(uint64_t) > SIZEOF_MEMBER(struct timeval, tv_sec)) goto invalid_cast;
			dst->datum.timeval.tv_sec = src->datum.integer64;
			dst->datum.timeval.tv_usec = 0;
			break;

		default:
			goto invalid_cast;
		}
	}

	/*
	 *	Conversions between IPv4 addresses, IPv6 addresses, IPv4 prefixes and IPv6 prefixes
	 *
	 *	For prefix to ipaddress conversions, we assume that the host portion has already
	 *	been zeroed out.
	 *
	 *	We allow casts from v6 to v4 if the v6 address has the correct mapping prefix.
	 *
	 *	We only allow casts from prefixes to addresses if the prefix is the the length of
	 *	the address, e.g. 32 for ipv4 128 for ipv6.
	 */
	{
		/*
		 *	10 bytes of 0x00 2 bytes of 0xff
		 */
		static uint8_t const v4_v6_map[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						     0x00, 0x00, 0x00, 0x00, 0xff, 0xff };

		switch (dst_type) {
		case PW_TYPE_IPV4_ADDR:
			switch (src->type) {
			case PW_TYPE_IPV6_ADDR:
				if (memcmp(src->datum.ipv6addr.s6_addr, v4_v6_map, sizeof(v4_v6_map)) != 0) {
				bad_v6_prefix_map:
					fr_strerror_printf("Invalid cast from %s to %s.  No IPv4-IPv6 mapping prefix",
							   fr_int2str(dict_attr_types, src->type, "<INVALID>"),
							   fr_int2str(dict_attr_types, dst_type, "<INVALID>"));
					return -1;
				}

				memcpy(&dst->datum.ipaddr, &src->datum.ipv6addr.s6_addr[sizeof(v4_v6_map)],
				       sizeof(dst->datum.ipaddr));
				goto fixed_length;

			case PW_TYPE_IPV4_PREFIX:
				if (src->datum.ipv4prefix[1] != 32) {
				bad_v4_prefix_len:
					fr_strerror_printf("Invalid cast from %s to %s.  Only /32 prefixes may be "
							   "cast to IP address types",
							   fr_int2str(dict_attr_types, src->type, "<INVALID>"),
							   fr_int2str(dict_attr_types, dst_type, "<INVALID>"));
					return -1;
				}

				memcpy(&dst->datum.ipaddr, &src->datum.ipv4prefix[2], sizeof(dst->datum.ipaddr));
				goto fixed_length;

			case PW_TYPE_IPV6_PREFIX:
				if (src->datum.ipv6prefix[1] != 128) {
				bad_v6_prefix_len:
					fr_strerror_printf("Invalid cast from %s to %s.  Only /128 prefixes may be "
							   "cast to IP address types",
							   fr_int2str(dict_attr_types, src->type, "<INVALID>"),
							   fr_int2str(dict_attr_types, dst_type, "<INVALID>"));
					return -1;
				}
				if (memcmp(&src->datum.ipv6prefix[2], v4_v6_map, sizeof(v4_v6_map)) != 0) {
					goto bad_v6_prefix_map;
				}
				memcpy(&dst->datum.ipaddr, &src->datum.ipv6prefix[2 + sizeof(v4_v6_map)],
				       sizeof(dst->datum.ipaddr));
				goto fixed_length;

			default:
				break;
			}
			break;

		case PW_TYPE_IPV6_ADDR:
			switch (src->type) {
			case PW_TYPE_IPV4_ADDR:
				/* Add the v4/v6 mapping prefix */
				memcpy(dst->datum.ipv6addr.s6_addr, v4_v6_map, sizeof(v4_v6_map));
				memcpy(&dst->datum.ipv6addr.s6_addr[sizeof(v4_v6_map)], &src->datum.ipaddr,
				       sizeof(dst->datum.ipv6addr.s6_addr) - sizeof(v4_v6_map));

				goto fixed_length;

			case PW_TYPE_IPV4_PREFIX:
				if (src->datum.ipv4prefix[1] != 32) goto bad_v4_prefix_len;

				/* Add the v4/v6 mapping prefix */
				memcpy(dst->datum.ipv6addr.s6_addr, v4_v6_map, sizeof(v4_v6_map));
				memcpy(&dst->datum.ipv6addr.s6_addr[sizeof(v4_v6_map)], &src->datum.ipv4prefix[2],
				       sizeof(dst->datum.ipv6addr.s6_addr) - sizeof(v4_v6_map));
				goto fixed_length;

			case PW_TYPE_IPV6_PREFIX:
				if (src->datum.ipv4prefix[1] != 128) goto bad_v6_prefix_len;

				memcpy(dst->datum.ipv6addr.s6_addr, &src->datum.ipv6prefix[2], sizeof(dst->datum.ipv6addr.s6_addr));
				goto fixed_length;

			default:
				break;
			}
			break;

		case PW_TYPE_IPV4_PREFIX:
			switch (src->type) {
			case PW_TYPE_IPV4_ADDR:
				memcpy(&dst->datum.ipv4prefix[2], &src->datum.ipaddr, sizeof(dst->datum.ipv4prefix) - 2);
				dst->datum.ipv4prefix[0] = 0;
				dst->datum.ipv4prefix[1] = 32;
				goto fixed_length;

			case PW_TYPE_IPV6_ADDR:
				if (memcmp(src->datum.ipv6addr.s6_addr, v4_v6_map, sizeof(v4_v6_map)) != 0) {
					goto bad_v6_prefix_map;
				}
				memcpy(&dst->datum.ipv4prefix[2], &src->datum.ipv6addr.s6_addr[sizeof(v4_v6_map)],
				       sizeof(dst->datum.ipv4prefix) - 2);
				dst->datum.ipv4prefix[0] = 0;
				dst->datum.ipv4prefix[1] = 32;
				goto fixed_length;

			case PW_TYPE_IPV6_PREFIX:
				if (memcmp(&src->datum.ipv6prefix[2], v4_v6_map, sizeof(v4_v6_map)) != 0) {
					goto bad_v6_prefix_map;
				}

				/*
				 *	Prefix must be >= 96 bits. If it's < 96 bytes and the
				 *	above check passed, the v6 address wasn't masked
				 *	correctly when it was packet into a value_box_t.
				 */
				if (!fr_cond_assert(src->datum.ipv6prefix[1] >= (sizeof(v4_v6_map) * 8))) return -1;

				memcpy(&dst->datum.ipv4prefix[2], &src->datum.ipv6prefix[2 + sizeof(v4_v6_map)],
				       sizeof(dst->datum.ipv4prefix) - 2);
				dst->datum.ipv4prefix[0] = 0;
				dst->datum.ipv4prefix[1] = src->datum.ipv6prefix[1] - (sizeof(v4_v6_map) * 8);
				goto fixed_length;

			default:
				break;
			}
			break;

		case PW_TYPE_IPV6_PREFIX:
			switch (src->type) {
			case PW_TYPE_IPV4_ADDR:
				/* Add the v4/v6 mapping prefix */
				memcpy(&dst->datum.ipv6prefix[2], v4_v6_map, sizeof(v4_v6_map));
				memcpy(&dst->datum.ipv6prefix[2 + sizeof(v4_v6_map)], &src->datum.ipaddr,
				       (sizeof(dst->datum.ipv6prefix) - 2) - sizeof(v4_v6_map));
				dst->datum.ipv6prefix[0] = 0;
				dst->datum.ipv6prefix[1] = 128;
				goto fixed_length;

			case PW_TYPE_IPV4_PREFIX:
				/* Add the v4/v6 mapping prefix */
				memcpy(&dst->datum.ipv6prefix[2], v4_v6_map, sizeof(v4_v6_map));
				memcpy(&dst->datum.ipv6prefix[2 + sizeof(v4_v6_map)], &src->datum.ipv4prefix[2],
				       (sizeof(dst->datum.ipv6prefix) - 2) - sizeof(v4_v6_map));
				dst->datum.ipv6prefix[0] = 0;
				dst->datum.ipv6prefix[1] = (sizeof(v4_v6_map) * 8) + src->datum.ipv4prefix[1];
				goto fixed_length;

			case PW_TYPE_IPV6_ADDR:
				memcpy(&dst->datum.ipv6prefix[2], &src->datum.ipv6addr, sizeof(dst->datum.ipv6prefix) - 2);
				dst->datum.ipv6prefix[0] = 0;
				dst->datum.ipv6prefix[1] = 128;
				goto fixed_length;

			default:
				break;
			}

			break;

		default:
			break;
		}
	}

	/*
	 *	The attribute we've found has to have a size which is
	 *	compatible with the type of the destination cast.
	 */
	if ((src->length < dict_attr_sizes[dst_type][0]) ||
	    (src->length > dict_attr_sizes[dst_type][1])) {
	    	char const *type_name;

	    	type_name = fr_int2str(dict_attr_types, src->type, "<INVALID>");
		fr_strerror_printf("Invalid cast from %s to %s. Length should be between %zu and %zu but is %zu",
				   type_name,
				   fr_int2str(dict_attr_types, dst_type, "<INVALID>"),
				   dict_attr_sizes[dst_type][0], dict_attr_sizes[dst_type][1],
				   src->length);
		return -1;
	}

	if (src->type == PW_TYPE_OCTETS) {
		value_box_t tmp;

	do_octets:
		if (src->length < value_box_field_sizes[dst_type]) {
			fr_strerror_printf("Invalid cast from %s to %s.  Source is length %zd is smaller than destination type size %zd",
					   fr_int2str(dict_attr_types, src->type, "<INVALID>"),
					   fr_int2str(dict_attr_types, dst_type, "<INVALID>"),
					   src->length,
					   value_box_field_sizes[dst_type]);
			return -1;
		}

		/*
		 *	Copy the raw octets into the datum of a value_box
		 *	inverting bytesex for integers (if LE).
		 */
		memcpy(&tmp.datum, src->datum.octets, value_box_field_sizes[dst_type]);
		tmp.type = dst_type;
		tmp.length = value_box_field_sizes[dst_type];
		if (fr_dict_enum_types[dst_type]) dst->datum.enumv = dst_enumv;

		value_box_hton(dst, &tmp);

		return 0;
	}

	/*
	 *	Convert host order to network byte order.
	 */
	if ((dst_type == PW_TYPE_IPV4_ADDR) &&
	    ((src->type == PW_TYPE_INTEGER) ||
	     (src->type == PW_TYPE_DATE) ||
	     (src->type == PW_TYPE_SIGNED))) {
		dst->datum.ipaddr.s_addr = htonl(src->datum.integer);

	} else if ((src->type == PW_TYPE_IPV4_ADDR) &&
		   ((dst_type == PW_TYPE_INTEGER) ||
		    (dst_type == PW_TYPE_DATE) ||
		    (dst_type == PW_TYPE_SIGNED))) {
		dst->datum.integer = htonl(src->datum.ipaddr.s_addr);

	} else {		/* they're of the same byte order */
		memcpy(&dst->datum, &src->datum, src->length);
	}

	dst->length = src->length;
	dst->type = dst_type;
	if (fr_dict_enum_types[dst_type]) dst->datum.enumv = dst_enumv;

	return 0;
}

/** Copy value data verbatim duplicating any buffers
 *
 * @param ctx To allocate buffers in.
 * @param dst Where to copy value_box to.
 * @param src Where to copy value_box from.
 * @return
 *	- 0 on success.
 *	- -1 on failure.
 */
int value_box_copy(TALLOC_CTX *ctx, value_box_t *dst, const value_box_t *src)
{
	if (!fr_cond_assert(src->type != PW_TYPE_INVALID)) return -1;

	switch (src->type) {
	default:
		memcpy(((uint8_t *)dst) + value_box_offsets[src->type],
		       ((uint8_t const *)src) + value_box_offsets[src->type],
		       value_box_field_sizes[src->type]);
		break;

	case PW_TYPE_STRING:
		dst->datum.strvalue = talloc_bstrndup(ctx, src->datum.strvalue, src->length);
		if (!dst->datum.strvalue) return -1;
		break;

	case PW_TYPE_OCTETS:
		dst->datum.octets = talloc_memdup(ctx, src->datum.octets, src->length);
		talloc_set_type(dst->datum.strvalue, uint8_t);
		if (!dst->datum.octets) return -1;
		break;
	}

	value_box_copy_attrs(dst, src);

	return 0;
}

/** Copy value data verbatim moving any buffers to the specified context
 *
 * @param ctx To allocate buffers in.
 * @param dst Where to copy value_box to.
 * @param src Where to copy value_box from.
 * @return
 *	- 0 on success.
 *	- -1 on failure.
 */
int value_box_steal(TALLOC_CTX *ctx, value_box_t *dst, const value_box_t *src)
{
	if (!fr_cond_assert(src->type != PW_TYPE_INVALID)) return -1;

	switch (src->type) {
	default:
		memcpy(dst, src, sizeof(*src));
		break;

	case PW_TYPE_STRING:
		dst->datum.strvalue = talloc_steal(ctx, src->datum.strvalue);
		dst->tainted = src->tainted;
		if (!dst->datum.strvalue) {
			fr_strerror_printf("Failed stealing string buffer");
			return -1;
		}
		break;

	case PW_TYPE_OCTETS:
		dst->datum.octets = talloc_steal(ctx, src->datum.octets);
		dst->tainted = src->tainted;
		if (!dst->datum.octets) {
			fr_strerror_printf("Failed stealing octets buffer");
			return -1;
		}
		break;
	}

	value_box_copy_attrs(dst, src);

	return 0;
}

/** Print one attribute value to a string
 *
 */
char *value_box_asprint(TALLOC_CTX *ctx, value_box_t const *data, char quote)
{
	char *p = NULL;

	if (!fr_cond_assert(data->type != PW_TYPE_INVALID)) return NULL;

	if (fr_dict_enum_types[data->type] && data->datum.enumv) {
		fr_dict_enum_t const	*dv;
		value_box_t		tmp;

		value_box_cast(ctx, &tmp, PW_TYPE_INTEGER, NULL, data);

		dv = fr_dict_enum_by_da(data->datum.enumv, tmp.datum.integer);
		if (dv) return talloc_typed_strdup(ctx, dv->name);
	}

	switch (data->type) {
	case PW_TYPE_STRING:
	{
		size_t len, ret;

		if (!quote) {
			p = talloc_bstrndup(ctx, data->datum.strvalue, data->length);
			if (!p) return NULL;
			talloc_set_type(p, char);
			return p;
		}

		/* Gets us the size of the buffer we need to alloc */
		len = fr_snprint_len(data->datum.strvalue, data->length, quote);
		p = talloc_array(ctx, char, len);
		if (!p) return NULL;

		ret = fr_snprint(p, len, data->datum.strvalue, data->length, quote);
		if (!fr_cond_assert(ret == (len - 1))) {
			talloc_free(p);
			return NULL;
		}
		break;
	}

	case PW_TYPE_BYTE:
		p = talloc_typed_asprintf(ctx, "%u", data->datum.byte);
		break;

	case PW_TYPE_SHORT:
		p = talloc_typed_asprintf(ctx, "%u", data->datum.ushort);
		break;

	case PW_TYPE_INTEGER:
		p = talloc_typed_asprintf(ctx, "%u", data->datum.integer);
		break;

	case PW_TYPE_INTEGER64:
		p = talloc_typed_asprintf(ctx, "%" PRIu64, data->datum.integer64);
		break;

	case PW_TYPE_SIZE:
		p = talloc_typed_asprintf(ctx, "%zu", data->datum.size);
		break;

	case PW_TYPE_SIGNED:
		p = talloc_typed_asprintf(ctx, "%d", data->datum.sinteger);
		break;

	case PW_TYPE_TIMEVAL:
		p = talloc_typed_asprintf(ctx, "%" PRIu64 ".%06" PRIu64,
					  (uint64_t)data->datum.timeval.tv_sec, (uint64_t)data->datum.timeval.tv_usec);
		break;

	case PW_TYPE_ETHERNET:
		p = talloc_typed_asprintf(ctx, "%02x:%02x:%02x:%02x:%02x:%02x",
					  data->datum.ether[0], data->datum.ether[1],
					  data->datum.ether[2], data->datum.ether[3],
					  data->datum.ether[4], data->datum.ether[5]);
		break;

	case PW_TYPE_ABINARY:
#ifdef WITH_ASCEND_BINARY
		p = talloc_array(ctx, char, 128);
		if (!p) return NULL;
		print_abinary(p, 128, (uint8_t const *) &data->datum.filter, data->length, 0);
		break;
#else
		  /* FALL THROUGH */
#endif

	case PW_TYPE_OCTETS:
		p = talloc_array(ctx, char, 2 + 1 + data->length * 2);
		if (!p) return NULL;
		p[0] = '0';
		p[1] = 'x';

		fr_bin2hex(p + 2, data->datum.octets, data->length);
		p[2 + (data->length * 2)] = '\0';
		break;

	case PW_TYPE_DATE:
	{
		time_t t;
		struct tm s_tm;

		t = data->datum.date;

		p = talloc_array(ctx, char, 64);
		strftime(p, 64, "%b %e %Y %H:%M:%S %Z",
			 localtime_r(&t, &s_tm));
		break;
	}

	/*
	 *	We need to use the proper inet_ntop functions for IP
	 *	addresses, else the output might not match output of
	 *	other functions, which makes testing difficult.
	 *
	 *	An example is tunneled ipv4 in ipv6 addresses.
	 */
	case PW_TYPE_IPV4_ADDR:
	case PW_TYPE_IPV4_PREFIX:
	{
		char buff[INET_ADDRSTRLEN  + 4]; // + /prefix

		buff[0] = '\0';
		value_box_snprint(buff, sizeof(buff), data, '\0');

		p = talloc_typed_strdup(ctx, buff);
	}
	break;

	case PW_TYPE_IPV6_ADDR:
	case PW_TYPE_IPV6_PREFIX:
	{
		char buff[INET6_ADDRSTRLEN + 4]; // + /prefix

		buff[0] = '\0';
		value_box_snprint(buff, sizeof(buff), data, '\0');

		p = talloc_typed_strdup(ctx, buff);
	}
	break;

	case PW_TYPE_IFID:
		p = talloc_typed_asprintf(ctx, "%x:%x:%x:%x",
					  (data->datum.ifid[0] << 8) | data->datum.ifid[1],
					  (data->datum.ifid[2] << 8) | data->datum.ifid[3],
					  (data->datum.ifid[4] << 8) | data->datum.ifid[5],
					  (data->datum.ifid[6] << 8) | data->datum.ifid[7]);
		break;

	case PW_TYPE_BOOLEAN:
		p = talloc_typed_strdup(ctx, data->datum.byte ? "yes" : "no");
		break;

	case PW_TYPE_DECIMAL:
		p = talloc_typed_asprintf(ctx, "%g", data->datum.decimal);
		break;

	/*
	 *	Don't add default here
	 */
	case PW_TYPE_COMBO_IP_ADDR:
	case PW_TYPE_COMBO_IP_PREFIX:
	case PW_TYPE_STRUCTURAL:
	case PW_TYPE_BAD:
		(void)fr_cond_assert(0);
		return NULL;
	}

	return p;
}

/** Print the value of an attribute to a string
 *
 * @note return value should be checked with is_truncated.
 * @note Will always \0 terminate unless outlen == 0.
 *
 * @param out Where to write the printed version of the attribute value.
 * @param outlen Length of the output buffer.
 * @param data to print.
 * @param quote char to escape in string output.
 * @return
 *	- The number of bytes written to the out buffer.
 *	- A number >= outlen if truncation has occurred.
 */
size_t value_box_snprint(char *out, size_t outlen, value_box_t const *data, char quote)
{
	char		buf[1024];	/* Interim buffer to use with poorly behaved printing functions */
	char const	*a = NULL;
	char		*p = out;
	time_t		t;
	struct tm	s_tm;

	size_t		len = 0, freespace = outlen;

	if (!fr_cond_assert(data->type != PW_TYPE_INVALID)) return -1;

	if (!data) return 0;
	if (outlen == 0) return data->length;

	*out = '\0';

	p = out;

	if (fr_dict_enum_types[data->type] && data->datum.enumv) {
		fr_dict_enum_t const	*dv;
		value_box_t		tmp;

		value_box_cast(NULL, &tmp, PW_TYPE_INTEGER, NULL, data);

		dv = fr_dict_enum_by_da(data->datum.enumv, tmp.datum.integer);
		if (dv) return strlcpy(out, dv->name, outlen);
	}

	switch (data->type) {
	case PW_TYPE_STRING:

		/*
		 *	Ensure that WE add the quotation marks around the string.
		 */
		if (quote) {
			if (freespace < 3) return data->length + 2;

			*p++ = quote;
			freespace--;

			len = fr_snprint(p, freespace, data->datum.strvalue, data->length, quote);
			/* always terminate the quoted string with another quote */
			if (len >= (freespace - 1)) {
				/* Use out not p as we're operating on the entire buffer */
				out[outlen - 2] = (char) quote;
				out[outlen - 1] = '\0';
				return len + 2;
			}
			p += len;
			freespace -= len;

			*p++ = (char) quote;
			freespace--;
			*p = '\0';

			return len + 2;
		}

		return fr_snprint(out, outlen, data->datum.strvalue, data->length, quote);

	case PW_TYPE_BYTE:
		return snprintf(out, outlen, "%u", data->datum.byte);

	case PW_TYPE_SHORT:
		return snprintf(out, outlen, "%u", data->datum.ushort);

	case PW_TYPE_INTEGER:
		return snprintf(out, outlen, "%u", data->datum.integer);

	case PW_TYPE_INTEGER64:
		return snprintf(out, outlen, "%" PRIu64, data->datum.integer64);

	case PW_TYPE_SIZE:
		return snprintf(out, outlen, "%zu", data->datum.size);

	case PW_TYPE_SIGNED: /* Damned code for 1 WiMAX attribute */
		len = snprintf(buf, sizeof(buf), "%d", data->datum.sinteger);
		a = buf;
		break;

	case PW_TYPE_TIMEVAL:
		len = snprintf(buf, sizeof(buf),  "%" PRIu64 ".%06" PRIu64,
			       (uint64_t)data->datum.timeval.tv_sec, (uint64_t)data->datum.timeval.tv_usec);
		a = buf;
		break;

	case PW_TYPE_DATE:
		t = data->datum.date;
		if (quote > 0) {
			len = strftime(buf, sizeof(buf) - 1, "%%%b %e %Y %H:%M:%S %Z%%", localtime_r(&t, &s_tm));
			buf[0] = (char) quote;
			buf[len - 1] = (char) quote;
			buf[len] = '\0';
		} else {
			len = strftime(buf, sizeof(buf), "%b %e %Y %H:%M:%S %Z", localtime_r(&t, &s_tm));
		}
		a = buf;
		break;

	case PW_TYPE_IPV4_ADDR:
		a = inet_ntop(AF_INET, &(data->datum.ipaddr), buf, sizeof(buf));
		len = strlen(buf);
		break;

	case PW_TYPE_ABINARY:
#ifdef WITH_ASCEND_BINARY
		print_abinary(buf, sizeof(buf), (uint8_t const *) data->datum.filter, data->length, quote);
		a = buf;
		len = strlen(buf);
		break;
#else
	/* FALL THROUGH */
#endif
	case PW_TYPE_OCTETS:
	case PW_TYPE_TLV:
	{
		size_t max;

		/* Return the number of bytes we would have written */
		len = (data->length * 2) + 2;
		if (freespace <= 1) {
			return len;
		}

		*out++ = '0';
		freespace--;

		if (freespace <= 1) {
			*out = '\0';
			return len;
		}
		*out++ = 'x';
		freespace--;

		if (freespace <= 2) {
			*out = '\0';
			return len;
		}

		/* Get maximum number of bytes we can encode given freespace */
		max = ((freespace % 2) ? freespace - 1 : freespace - 2) / 2;
		fr_bin2hex(out, data->datum.octets, ((size_t)data->length > max) ? max : (size_t)data->length);
	}
		return len;

	case PW_TYPE_IFID:
		a = fr_inet_ifid_ntop(buf, sizeof(buf), data->datum.ifid);
		len = strlen(buf);
		break;

	case PW_TYPE_IPV6_ADDR:
		a = inet_ntop(AF_INET6, &data->datum.ipv6addr, buf, sizeof(buf));
		len = strlen(buf);
		break;

	case PW_TYPE_IPV6_PREFIX:
	{
		struct in6_addr addr;

		/*
		 *	Alignment issues.
		 */
		memcpy(&addr, &(data->datum.ipv6prefix[2]), sizeof(addr));

		a = inet_ntop(AF_INET6, &addr, buf, sizeof(buf));
		if (a) {
			p = buf;

			len = strlen(buf);
			p += len;
			len += snprintf(p, sizeof(buf) - len, "/%u", (unsigned int) data->datum.ipv6prefix[1]);
		}
	}
		break;

	case PW_TYPE_IPV4_PREFIX:
	{
		struct in_addr addr;

		/*
		 *	Alignment issues.
		 */
		memcpy(&addr, &(data->datum.ipv4prefix[2]), sizeof(addr));

		a = inet_ntop(AF_INET, &addr, buf, sizeof(buf));
		if (a) {
			p = buf;

			len = strlen(buf);
			p += len;
			len += snprintf(p, sizeof(buf) - len, "/%u", (unsigned int) (data->datum.ipv4prefix[1] & 0x3f));
		}
	}
		break;

	case PW_TYPE_ETHERNET:
		return snprintf(out, outlen, "%02x:%02x:%02x:%02x:%02x:%02x",
				data->datum.ether[0], data->datum.ether[1],
				data->datum.ether[2], data->datum.ether[3],
				data->datum.ether[4], data->datum.ether[5]);

	case PW_TYPE_DECIMAL:
		return snprintf(out, outlen, "%g", data->datum.decimal);

	/*
	 *	Don't add default here
	 */
	case PW_TYPE_INVALID:
	case PW_TYPE_COMBO_IP_ADDR:
	case PW_TYPE_COMBO_IP_PREFIX:
	case PW_TYPE_EXTENDED:
	case PW_TYPE_LONG_EXTENDED:
	case PW_TYPE_EVS:
	case PW_TYPE_VSA:
	case PW_TYPE_VENDOR:
	case PW_TYPE_BOOLEAN:
	case PW_TYPE_STRUCT:
	case PW_TYPE_MAX:
		(void)fr_cond_assert(0);
		*out = '\0';
		return 0;
	}

	if (a) strlcpy(out, a, outlen);

	return len;	/* Return the number of bytes we would of written (for truncation detection) */
}

