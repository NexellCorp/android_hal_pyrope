/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2006-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <mali_system.h>

#include <base/mali_runtime.h>

#include <base/mali_debug.h>
#include <shared/binary_shader/bs_symbol.h>

/****************************** ATOI ***************************/

/* works just like the normal atoi */
MALI_STATIC int bs_atoi(const char* str)
{
	int retval = 0;
	int multiplier = 1;
	/* handle leading spaces */
	while(*str == ' ') str++;

	/* handle sign */
	if(*str == '-')
		{
			multiplier = -1;
			str++;
		}

	/* parse numbers */
	while(*str != '\0')
	{
			/* end at non-digits */
			if(*str < '0' || *str > '9') break;

			/* increase number */
			retval *= 10;
			retval += *str - '0';
			str++;
	}
	return retval * multiplier;
}

/***************************** DELETE **************************/

/* frees all contents in a symbol_table, but not the table itself */
MALI_STATIC void recursive_delete_symbols(struct bs_symbol_table* table)
{
	int i;

	MALI_DEBUG_ASSERT_POINTER( table );

	if (NULL != table->members)
	{
		for(i = 0; i < (int)table->member_count; i++)
		{
			bs_symbol_free( table->members[i] );
		}

		_mali_sys_free(table->members);
		table->members = NULL;
	}

	table->member_count = 0;
}

/* frees one symbol and all content */
MALI_EXPORT void bs_symbol_free(struct bs_symbol* symbol)
{
	/* handle deletion of NULL-objects */
	if(NULL == symbol) return;

	if(symbol->datatype == DATATYPE_STRUCT) recursive_delete_symbols(&symbol->type.construct);
	_mali_sys_free(symbol->name);
	_mali_sys_free(symbol);
}

/* allocates one symbol - initializes all values to NULL, but you may want to override this */
MALI_EXPORT struct bs_symbol* bs_symbol_alloc(const char* name)
{
	struct bs_symbol* retval;
	u32 namelen;

	/* allocate symbol */
	retval = _mali_sys_malloc(sizeof(struct bs_symbol));
	if (NULL == retval) return NULL;

	/* init object */
	_mali_sys_memset(retval, 0, sizeof(struct bs_symbol));

	/* copy name */
	if (NULL != name)
	{
		namelen = _mali_sys_strlen(name);
		retval->name = _mali_sys_malloc(namelen + 1);
		if (NULL == retval->name)
		{
			_mali_sys_free(retval);
			return NULL;
		}
		_mali_sys_memcpy(retval->name, name, namelen + 1);
	}
	return retval;
}

/* frees one symbol table and all content */
MALI_EXPORT void bs_symbol_table_free(struct bs_symbol_table* table)
{
	MALI_DEBUG_ASSERT_POINTER( table );
	recursive_delete_symbols(table);
	_mali_sys_free(table);
}

/* allocates a new symbol table with toplevel_elements available slots on the top level */
MALI_EXPORT struct bs_symbol_table* bs_symbol_table_alloc(u32 toplevel_elements)
{
	struct bs_symbol_table* retval;

	/* allocate table */
	retval = _mali_sys_malloc(sizeof(struct bs_symbol_table));
	if(!retval) return NULL;

	/* keep track of member count */
	retval->member_count = toplevel_elements;

	if( toplevel_elements > 0 )
	{
		/* allocate member array */
		retval->members = _mali_sys_malloc(toplevel_elements * sizeof(struct bs_symbol*));
		if (NULL == retval->members)
		{
			_mali_sys_free(retval);
			return NULL;
		}
		/* null all members */
		_mali_sys_memset(retval->members, 0, toplevel_elements * sizeof(struct bs_symbol*));
	} else {
		retval->members = NULL;
	}

	return retval;
}

/***************************** LOOKUP **************************/
/* @brief A helper function to parse a uniform name string into a substring that can be matched to symbol names.
 * @param name The variable or construct name you want to locate, f.ex "mystruct[8].membersomething"
 * @param indexnumber Returns the index into the symbol array that matches the output name. This location must be valid and contain zero on entry.
 * @param indexnumber Returns the position of the first dot in the name. This location must be valid and contain -1 on entry. Returns : -1 if the name contains no dot.
 * @return The length of the substring in name that should match a symbol name.
 */
MALI_STATIC s32 bs_parse_lookup_name( const char* name, int* indexnumber, s32* dot_position)
{
	u32 input_namelen = _mali_sys_strlen(name);      /* length of the name given */
	s32 retval = input_namelen;				/* real length of the symbol name (without brackets and stuff) */
	s32 bracketlevel= 0;                   /* the amount of nested brackets. Can only be 0 or 1 without exiting */
	s32 num_brackets_found = 0;            /* increases every time we find a new bracket, never decreases */
	u32 i;                                 /* simple counter iterating all characters, increasing for each char covered */

	/* empty string means we're done */
	if(input_namelen == 0) return 0;

	MALI_DEBUG_ASSERT_POINTER( indexnumber );
	MALI_DEBUG_ASSERT_POINTER( dot_position );

	MALI_DEBUG_ASSERT( *indexnumber == 0, ("Parameter indexnumber in function bs_parse_lookup_name must point to an int equal to zero on entry."));
	MALI_DEBUG_ASSERT( *dot_position == -1, ("Parameter dot_position in function bs_parse_lookup_name must point to an s32 equal to -1 on entry."));

	/* state through all letters until we find a dot*/
	for(i = 0; i < input_namelen; i++)
	{
		char c = name[i];
		switch(c)
		{
			case '.':
				/* We found a dot. This means we do a recursion - if we find the symbol we're looking for */
				*dot_position = i;
				/* save (shorten) the position where we stopped finding the symbol name - unless already done!*/
				if(retval == (s32)input_namelen) retval = i;
				/* check for open brackets */
				if(bracketlevel != 0) return 0;
				return retval;
			case '[':
				/* we found a beginning bracket */
				num_brackets_found++;
				if(num_brackets_found > 1) return 0; /* we only allow single arrays */
				retval = i; /* save (shorten) the position where we stopped finding the symbol name */
				bracketlevel++;
				if(bracketlevel > 1) return 0; /* we do not allow nested brackets */
				break; /* break switch */
			case ']':
				/* we found an ending bracket */
				bracketlevel--;
				if(bracketlevel < 0) return 0; /* mismatched bracket closure */
				if(i > 0 && name[i-1] == '[') return 0; /* if last character was a begin bracket, we fail */
				/* at this point, get the index number in the bracket */
				*indexnumber = bs_atoi(&name[retval+1]);
				break; /* break switch */
			default:
				/* we found a regular character */
				/* if inside an index, we only allow numerical characters */
				if(bracketlevel > 0)
				{
					switch(c)
					{
						case '0':
						case '1':
						case '2':
						case '3':
						case '4':
						case '5':
						case '6':
						case '7':
						case '8':
						case '9':
								break;
						default: return 0; /* only allow digits */
					}
				}
				/* otherwise, everything is ok! */
		}
	}
	/* unterminated brackets is a nono */
	if(bracketlevel != 0) return 0;
	return retval;
}

/* @brief recursive version of bs_lookup_symbol. It's recursive to handle structures.
 * @param table The table to lookup into
 * @param name The variable or construct name you want to locate, f.ex "mystruct[8].membersomething"
 * @param vertex_offset, fragment_offset, sampler_offset Output variables of this function, location of the found symbol in the vertex and fragment shader, or -1 if not found.
 * @return A binary shader symbol if located, or NULL if not.
 */
MALI_STATIC struct bs_symbol* recursive_lookup_symbol(struct bs_symbol_table* table, const char* name,
                                                      s32* vertex_offset, s32* fragment_offset, s32* sampler_offset)
{

	int indexnumber = 0;                   /* the index number in the string, eg "fisk[32].foo[8].bar" would make this 32 */
	s32 dot_position = -1;					/* if we have a dot in the string, its location is placed here */

	s32 symbol_namelen = bs_parse_lookup_name(name, &indexnumber, &dot_position);    /* real length of the symbol name (without brackets and stuff) */

	if( symbol_namelen == 0 )return NULL;

	/* We've got a simple name to match. Time to get the symbol */
	{
		struct bs_symbol* retval = NULL;
		u32 i;

		/* locate the symbol given in front of the bracket */
		for(i = 0; i < table->member_count; i++)
		{
			/* some symbols MAY be NULL - skip them*/
			if(table->members[i] == NULL) continue;

			/* in addition to strncmp we need to check if symbol has a terminating zero at position 'namelen'
			 * and that the symbol found is a struct */
			if ( _mali_sys_memcmp( name, table->members[i]->name, symbol_namelen ) == 0 &&
				 table->members[i]->name[symbol_namelen] == '\0')
			{
				MALI_DEBUG_ASSERT(symbol_namelen != 0, ("We found a match for a symbol without any name. What?"));
				if(dot_position != -1 && table->members[i]->datatype != DATATYPE_STRUCT ) return NULL; /* we are looking for a struct node */
				retval = table->members[i];
				break; /* break inner for loop */
			} else {
				/* we considered a symbol, but its name did not match what we needed
				 * We will still need to count the number of samplers in this symbol though */
				if( table->members[i]->datatype == DATATYPE_STRUCT)
				{
					*sampler_offset += (bs_symbol_count_samplers( &table->members[i]->type.construct ) * table->members[i]->array_size);
				}
				if( bs_symbol_is_sampler( table->members[i] ) == MALI_TRUE ) *sampler_offset += MAX(table->members[i]->array_size, 1);
			}
		}
		if ( !retval ) return NULL; /* we found no node with this name - abort! */

		/* need sane index */
		if(indexnumber < 0) return NULL;
		if(indexnumber != 0 && (u32)indexnumber >= retval->array_size) return NULL;

		/* get index offsets from the symbol just located */
		if(retval->location.vertex >= 0)
		{
			*vertex_offset += retval->location.vertex + retval->array_element_stride.vertex* indexnumber;
		} else {
			*vertex_offset = -1;
		}
		if(retval->location.fragment >= 0)
		{
			*fragment_offset += retval->location.fragment + retval->array_element_stride.fragment* indexnumber;
		} else {
			*fragment_offset = -1;
		}

		if(retval->datatype == DATATYPE_STRUCT)
			*sampler_offset += bs_symbol_count_samplers( &retval->type.construct ) * indexnumber;

		if( bs_symbol_is_sampler( retval ) ) *sampler_offset += indexnumber;

		if(dot_position == -1)
		{
			if(retval->datatype == DATATYPE_STRUCT) return NULL; /* cannot lookup struct values directly */
			return retval;
		}

		/* get the subchild indices */
		/* some compilers do better if the recursive call is at the end of the function */
		return recursive_lookup_symbol(&retval->type.construct, &name[dot_position+1], vertex_offset, fragment_offset, sampler_offset);
	}
}

/**
 * @brief Check if the symbol name matches any of the given filters.
 * @param symbol Symbol to check 
 * @param filters Filter list
 * @param filter_count Filter list size
 * @return MALI_TRUE if the symbol name matches a filter, MALI_FALSE otherwise
 */
MALI_STATIC_FORCE_INLINE mali_bool bs_filter_symbol(struct bs_symbol* symbol, const char** filters, s32 filter_count)
{
	s32 k;
	u32 namelen = _mali_sys_strlen(symbol->name );

	for(k = 0; k < filter_count; k++)
	{
		u32 filterlen = _mali_sys_strlen(filters[k]);
		if( namelen >= filterlen && _mali_sys_memcmp(symbol->name, filters[k], filterlen) == 0 ) 
			return MALI_TRUE;
	}
	return MALI_FALSE;
}

/* @brief A recursive function that parses a symbol table in the same order that a uniform location table does. 
 *  It returns the index into a uniform location table that should contain the uniform refered to by this name. The uniform location table does not need to exist.
 * @param table The table to lookup into
 * @param name The variable or construct name you want to locate, f.ex "mystruct[8].membersomething"
 * @param filters An array of namefilters to remove from the search! Each filter is basically a prefix.
 * @param filtercount The number of filters given
 * @return An index into a uniform location table if located, or -1 if not.
 * 
 */
MALI_EXPORT s32 bs_lookup_uniform_location(struct bs_symbol_table* table, const char* name, const char** filters, int filtercount)
{
	s32 n = 0;

	int indexnumber = 0;                   /* the index number in the string, eg "fisk[32].foo[8].bar" would make this 32 */
	s32 dot_position = -1;					/* if we have a dot in the string, its location is placed here */

	s32 symbol_namelen = bs_parse_lookup_name(name, &indexnumber, &dot_position);    /* real length of the symbol name (without brackets and stuff) */

	if( symbol_namelen == 0 )return -1;

	/* We've got a simple name to match. Time to get the symbol */
	{
		struct bs_symbol* matched_symbol = NULL;
		u32 i;
		int ntmp;

		/* locate the symbol given in front of the bracket */
		for(i = 0; i < table->member_count; i++)
		{
			struct bs_symbol* symbol = table->members[i];

			/* some symbols MAY be NULL - skip them */
			if(symbol == NULL) continue;

			if( symbol->datatype == DATATYPE_STRUCT) continue;

			/* apply filtering */
			if (bs_filter_symbol(symbol, filters, filtercount))
				continue;
			
			/* in addition to strncmp we need to check if symbol has a terminating zero at position 'namelen'
			 * and that the symbol found is a struct */
			if ( _mali_sys_memcmp( name, symbol->name, symbol_namelen ) == 0 &&
				 symbol->name[symbol_namelen] == '\0')
			{
				MALI_DEBUG_ASSERT(symbol_namelen != 0, ("We found a match for a symbol without any name. What?"));
				if(dot_position != -1 ) return -1; /* we are looking for a struct node */
				matched_symbol = symbol;
				break; /* break inner for loop */
			} else {
				/* we considered a symbol, but its name did not match what we needed
				 * We will still need to add these to our index total */
				n +=  (!symbol->array_size)?1:symbol->array_size;

			}
		}
		if(!matched_symbol)
		{
			for(i = 0; i < table->member_count; i++)
			{
				struct bs_symbol* symbol = table->members[i];

				/* some symbols MAY be NULL - skip them*/
				if(symbol == NULL) continue;

				if( symbol->datatype != DATATYPE_STRUCT) continue;
				
				/* apply filtering */
				if (bs_filter_symbol(symbol, filters, filtercount))
					continue;

				/* in addition to strncmp we need to check if symbol has a terminating zero at position 'namelen'
				 * and that the symbol found is a struct */
				if ( _mali_sys_memcmp( name, symbol->name, symbol_namelen ) == 0 &&
					 symbol->name[symbol_namelen] == '\0')
				{
					MALI_DEBUG_ASSERT(symbol_namelen != 0, ("We found a match for a symbol without any name. What?"));
					matched_symbol = symbol;
					break; /* break inner for loop */
				} else {
					/* we considered a symbol, but its name did not match what we needed
					 * We will still need to count the number of location in this symbol though */
					n += bs_symbol_count_locations( &symbol->type.construct, filters, filtercount ) 
					      * 
						 ( (!symbol->array_size)?1:symbol->array_size) ;
				}
			}
		}
		if ( !matched_symbol ) return -1; /* we found no node with this name - abort! */

		/* need sane index */
		if(indexnumber < 0) return -1;
		if(indexnumber != 0 && (u32)indexnumber >= matched_symbol->array_size) return -1;

		/* if the index number is more than zero we need to add the locations from all previous array members */
		if(matched_symbol->datatype == DATATYPE_STRUCT)
		{
			if(indexnumber > 0)
				n += bs_symbol_count_locations( &matched_symbol->type.construct, filters, filtercount ) * indexnumber;
		}
		else
			n += indexnumber;

		if(dot_position == -1)
		{
			if(matched_symbol->datatype == DATATYPE_STRUCT) return -1; /* cannot lookup struct values directly */
			return n;
		}

		/* get the subchild indices */
		ntmp = bs_lookup_uniform_location(&matched_symbol->type.construct, &name[dot_position+1], filters, filtercount);
		return ( -1 == ntmp)? -1 : n + ntmp;
	}
}

MALI_EXPORT struct bs_symbol* bs_symbol_lookup(struct bs_symbol_table* table, const char* name, s32* vertex_offset, s32* fragment_offset, s32* sampler_offset)
{
	struct bs_symbol* retval = NULL;

	/* these two are dummy offset targets; allowing an easier implementation of recursive_lookup_table */
	s32 vfoo = 0;
	s32 ffoo = 0;
	s32 sfoo = 0;
	if(!vertex_offset) vertex_offset = &vfoo;
	if(!fragment_offset) fragment_offset = &ffoo;
	if(!sampler_offset) sampler_offset = &sfoo;

	/* handle NULL table */
	if(table == NULL)
	{
		*vertex_offset = -1;
		*fragment_offset = -1;
		*sampler_offset = -1;
		return NULL;
	}

	MALI_DEBUG_ASSERT_POINTER( name );

	*vertex_offset = 0;
	*fragment_offset = 0;
	*sampler_offset = 0;
	retval = recursive_lookup_symbol(table, name, vertex_offset, fragment_offset, sampler_offset);
	if( retval == NULL )
	{
		*vertex_offset = -1;
		*fragment_offset = -1;
		*sampler_offset = -1;
		return NULL;
	}
	if( !bs_symbol_is_sampler(retval) ) *sampler_offset = -1;
	return retval;
}

/******************************** COUNT *********************************/

MALI_EXPORT u32 bs_symbol_longest_location_name_length(struct bs_symbol_table* table)
{
	int retval = 0;
	u32 i;

	if(table == NULL) return 0; /* handle null tables */

	/* iterate all symbols, find "longest" symbol (accounting for arrays and structs) */
	for(i = 0; i < table->member_count; i++)
	{
		int size;
		struct bs_symbol* symbol = table->members[i];
		int arraycount = symbol->array_size;

		/* length of symbol is initially equal to symbol name length */
		size = _mali_sys_strlen(symbol->name);

		/* if this is an array; add length of longest array index plus length of two brackets ("[]") */
		if(arraycount)
		{
			char buffer[32];
			_mali_sys_snprintf(buffer, 32, "%i", arraycount);
			size += _mali_sys_strlen(buffer) + 2;
		}

		/* if this is a struct, add length of longest child, plus length of the dot */
		if(symbol->datatype == DATATYPE_STRUCT)
		{
			size += bs_symbol_longest_location_name_length(&symbol->type.construct) + 1;
		}

		/* If symbol name is big enough, remember it */
		if(size > retval) retval = size;
	}

	return retval;
}

MALI_EXPORT u32 bs_symbol_count_locations(struct bs_symbol_table* table, const char** filters, int filtercount)
{
	u32 i;
	int retval = 0;
	if(table == NULL) return 0; /* handle null tables */

	for(i = 0; i < table->member_count; i++)
	{
		/* silently ignore null members */
		if(table->members[i] == NULL) continue;

		/** attempt to filter out this symbol */
		if (bs_filter_symbol(table->members[i], filters, filtercount))
			continue;

		/* not filtered, count away */
		if(table->members[i]->datatype == DATATYPE_STRUCT)
		{
			int arraysize = (!table->members[i]->array_size)?1:(table->members[i]->array_size);
			retval += bs_symbol_count_locations(&table->members[i]->type.construct, filters, filtercount) * arraysize;
		} else {
			retval += (!table->members[i]->array_size)?1:(table->members[i]->array_size);
		}
	}
	return retval;
}

MALI_EXPORT u32 bs_symbol_count_samplers(struct bs_symbol_table* table)
{
	u32 i;
	int retval = 0;
	if(table == NULL) return 0; /* handle null tables */

	for(i = 0; i < table->member_count; i++)
	{
		if (DATATYPE_STRUCT == table->members[i]->datatype)
		{
			int arraysize = (!table->members[i]->array_size)?1:(table->members[i]->array_size);
			retval += bs_symbol_count_samplers(&table->members[i]->type.construct) * arraysize;
		}
		else if (bs_symbol_is_sampler(table->members[i]))
		{
			retval += (!table->members[i]->array_size)?1:(table->members[i]->array_size);
		}
	}
	return retval;
}

MALI_EXPORT u32 bs_symbol_count_actives(struct bs_symbol_table* table, const char** filters, int filtercount)
{
	u32 i;
	int retval = 0;
	if(table == NULL) return 0; /* handle null tables */

	for(i = 0; i < table->member_count; i++)
	{
		/* silently ignore null members */
		if(table->members[i] == NULL) continue;

		/** attempt to filter out this symbol */
		if (bs_filter_symbol(table->members[i], filters, filtercount))
			continue;

		/* not filtered, count away */
		if(table->members[i]->datatype == DATATYPE_STRUCT)
		{
			int arraysize = (!table->members[i]->array_size)?1:(table->members[i]->array_size);
			retval += bs_symbol_count_actives(&table->members[i]->type.construct, filters, filtercount) * arraysize;
		} else {
			retval += 1;
		}
	}
	return retval;
}

/*********************** SYMBOL SIZES *****************************/

MALI_EXPORT void bs_update_symbol_block_size(struct bs_symbol* symbol)
{
	symbol->block_size.vertex = 0;
	symbol->block_size.fragment = 0;

	if(symbol->datatype != DATATYPE_STRUCT)
	{
		/* primary datatypes */
		if(symbol->array_size != 0)
		{
			symbol->block_size.vertex += symbol->array_element_stride.vertex * (symbol->array_size -1);
			symbol->block_size.fragment += symbol->array_element_stride.fragment * (symbol->array_size -1);
		}
		if(symbol->datatype == DATATYPE_MATRIX)
		{
			symbol->block_size.vertex += symbol->type.primary.vector_stride.vertex * (symbol->type.primary.vector_size - 1);
			symbol->block_size.fragment += symbol->type.primary.vector_stride.fragment * (symbol->type.primary.vector_size - 1);
		}
		switch(symbol->datatype)
		{
		case DATATYPE_SAMPLER:
		case DATATYPE_SAMPLER_CUBE:
		case DATATYPE_SAMPLER_SHADOW:
			symbol->block_size.vertex += 1;
			symbol->block_size.fragment += 1;
			break;
		case DATATYPE_SAMPLER_EXTERNAL:
			symbol->block_size.vertex += 3;
			symbol->block_size.fragment += 3;
			break;
		default:
			symbol->block_size.vertex += symbol->type.primary.vector_size;
			symbol->block_size.fragment += symbol->type.primary.vector_size;
		}
	} else {
		/* Structs. Let's do it the easy way */
		if(!symbol->array_size)
		{
			symbol->block_size.vertex = symbol->array_element_stride.vertex;
			symbol->block_size.fragment = symbol->array_element_stride.fragment;
		} else {
			symbol->block_size.vertex = symbol->array_element_stride.vertex * symbol->array_size;
			symbol->block_size.fragment= symbol->array_element_stride.fragment * symbol->array_size;
		}

	}

}

/*********************** GET_NTH_LOCATION **************************/

/**** HELPER MACROS ****
 * namebuffer: Buffer holding name, given by caller
 * namebuffersize : Size of this buffer
 * str : String you want to add to the buffer
 * strlen : Length of this string, NOT including termchar
 *
 * namelen : Number of bytes used in namebuffer already - NOT including termchar
 *
 * buf_bytes_avail : Space left in buffer
 *           If buffer is 5 bytes big, and namelen is 2, then we have room for 3 more bytes in the buffer. Easy.
 * buf_bytes_to_be_written: How many bytes we want to write to the buffer
 *           If we have str_bytes_avail = 10, our string is 8 bytes long, we write 8 bytes + 1 byte termchar; ie 9 bytes.
 *           Same case, string is 10 bytes long, we write 9 bytes + 1 byte termchar; ie 10 bytes.
 *           If string is 20 bytes long we write 9 bytes + 1 byte termchar; again 10 bytes.
 *           If string is 2 bytes long, we write 2 bytes + 1 byte termchar; 3 bytes in total. Easy.
 *
 * Function modifies strlen if adding less than the required amount of bytesm - so that we can remove the correct number afterwards
 */
#define ADD_STRING_TO_BUFFER(str, strlen) \
{ \
	s32 buf_bytes_to_be_written; \
	s32 buf_bytes_avail = namebuffersize - namelen ; \
	if(buf_bytes_avail < 0) buf_bytes_avail = 0; \
	buf_bytes_to_be_written = ((s32)strlen+1 < buf_bytes_avail)?(s32)strlen+1:buf_bytes_avail; \
	_mali_sys_memcpy(namebuffer + namelen, str, buf_bytes_to_be_written); \
	if(buf_bytes_avail > 0) namelen += buf_bytes_to_be_written - 1 ; \
	if(buf_bytes_avail > 0) \
	{ \
		strlen = buf_bytes_to_be_written - 1; \
	} else { \
		strlen = 0; \
	} \
	if(buf_bytes_avail > 0) namebuffer[namelen] = '\0'; \
}

#define REMOVE_STRING_FROM_BUFFER(strlen) \
{ \
	namelen -= strlen; \
	if(namelen < namebuffersize) namebuffer[namelen] = '\0'; \
}

/* @brief recursive version of the non-wrapped version below; we have this version to easen the interface to the real function.
 * @param table The table to look up in
 * @param n This is the numbered location we want, will decrement as we find locations. Once it hits 0, we're done.
 * @param loc The location of s of a bs_uniform_location struct to be filled. The symbol member is not filled and should be filled with the returned symbol.
 * @param filters An array of namefilters to remove from the search! Each filter is basically a prefix.
 * @param filtercount The number of filters given.
 * @return A pointer to the symbol reffered to from this location.
*/
MALI_STATIC struct bs_symbol* wrap_bs_symbol_get_nth_location(struct bs_symbol_table* table, u32* pn, struct bs_uniform_location* loc, const char** filters, int filtercount)
{
	u32 i;

	MALI_DEBUG_ASSERT_POINTER( table );
	MALI_DEBUG_ASSERT_POINTER( pn );

	/* in order to mimic the bs_symbol_get_nth_sampler function, we must pass through the data twice;
	 * once for all nonstructs, then for all structs This first iteration is for all nonstructs */
	for(i = 0; i < table->member_count; i++)
	{
		struct bs_symbol* symbol = table->members[i];

 		/* silently ignore null members */
		if(symbol == NULL) continue;

		if( symbol->datatype == DATATYPE_STRUCT ) continue;

		/** attempt to filter out this symbol */
		if (bs_filter_symbol(symbol, filters, filtercount))
			continue;

		{
			u32 arraysize = (!symbol->array_size)?1:symbol->array_size;
			mali_bool is_sampler = bs_symbol_is_sampler(symbol);
			mali_bool is_vertex = (symbol->location.vertex >= 0);
			mali_bool is_fragment= (symbol->location.fragment >= 0);

			/* early out if n is large enough to cover all locations in this symbol */
			if(arraysize <= *pn)
			{
				*pn -= arraysize;
				if(is_sampler) loc->extra.sampler_location += arraysize;
				continue;
			}

			if(is_vertex)  loc->reg_location.vertex += symbol->location.vertex;
			if(is_fragment) loc->reg_location.fragment += symbol->location.fragment;
			if(is_sampler) loc->extra.sampler_location += *pn;
			loc->extra.base_index = 0;

			/* do we have an actual array? */
			if(symbol->array_size)
			{
				if(is_vertex) loc->reg_location.vertex += symbol->array_element_stride.vertex * (*pn);
				if(is_fragment) loc->reg_location.fragment += symbol->array_element_stride.fragment * (*pn);
				/* base offset for all types, including samplers */
				loc->extra.base_index = *pn;
			}

			/* override return values based on symbol capabilities */
			if(!is_vertex) loc->reg_location.vertex =-1;
			if(!is_fragment) loc->reg_location.fragment = -1;
			if(!is_sampler) loc->extra.sampler_location = -1;
			return symbol;

		} /* scoping */
	} /* for all symbols */

	/* next, repeat the process for all symbols */
	for(i = 0; i < table->member_count; i++)
	{
		struct bs_symbol* symbol = table->members[i];
 		/* silently ignore null members */
		if(symbol == NULL) continue;

		if( symbol->datatype != DATATYPE_STRUCT ) continue;

		/** attempt to filter out this symbol */
		if (bs_filter_symbol(symbol, filters, filtercount))
			continue;

		{
			u32 arraysize = (!symbol->array_size)?1:symbol->array_size;
			mali_bool is_vertex = (symbol->location.vertex >= 0);
			mali_bool is_fragment= (symbol->location.fragment >= 0);
			u32 c;

			if(is_vertex)   loc->reg_location.vertex += symbol->location.vertex;
			if(is_fragment) loc->reg_location.fragment += symbol->location.fragment;

			/* find one location per array count */
			for(c = 0; c < arraysize; c++)
			{
				struct bs_symbol* retval = NULL;

				/* do we have an actual array? */
				if(symbol->array_size)
				{
					if(is_vertex) loc->reg_location.vertex += symbol->array_element_stride.vertex * c;
					if(is_fragment) loc->reg_location.fragment += symbol->array_element_stride.fragment* c;
				}

				retval = wrap_bs_symbol_get_nth_location(&symbol->type.construct, pn, loc, filters, filtercount);
				if(NULL != retval) return retval;  /* we found it! */

				/* undo any array stuff */
				if(symbol->array_size)
				{
					if(is_vertex) loc->reg_location.vertex -= symbol->array_element_stride.vertex * c;
					if(is_fragment) loc->reg_location.fragment -= symbol->array_element_stride.fragment * c;
				}
			}
			if(is_vertex)   loc->reg_location.vertex -= symbol->location.vertex;
			if(is_fragment) loc->reg_location.fragment -= symbol->location.fragment;
		} /* scoping */
	} /* end for all symbols */

	return NULL; /* not found */
}

/* @brief recursive function that traverses the symbol table and fills the uniform location table.
 * @param table The table to look up in
 * @param loc The location of an array of bs_uniform_location structures.
 *			The struct pointed to by this parameter must be filled with the relative offsets on entry. These are zero at the beginning of the table.
 * @param vertex_location, fragment_location The shader locations of the location found.
 * @param filters An array of namefilters to remove from the search! Each filter is basically a prefix.
 * @param filtercount The number of filters given
*/
MALI_STATIC u32  recursive_bs_symbol_fill_location_table(struct bs_symbol_table* table, int num_locations, s32* psampler_count, struct bs_uniform_location* loc, const char** filters, int filtercount)
{
	u32 i;
	u32 n = 0;
	s32 vert_base		= loc->reg_location.vertex;
	s32 frag_base		= loc->reg_location.fragment;
	s32 sampler_location = *psampler_count;

	MALI_DEBUG_ASSERT_POINTER( table );
	MALI_DEBUG_ASSERT_POINTER( loc );

	/* in order to mimic the bs_symbol_get_nth_sampler function, we must pass through the data twice;
	 * once for all nonstructs, then for all structs This first iteration is for all nonstructs */
	for(i = 0; i < table->member_count; i++)
	{
		struct bs_symbol* symbol = table->members[i];

		/* silently ignore null members */
		if(symbol == NULL) continue;

		if(symbol->datatype == DATATYPE_STRUCT) continue;

		/** attempt to filter out this symbol */
		if (bs_filter_symbol(symbol, filters, filtercount))
			continue;
		
		{
			u32 arraysize = (!symbol->array_size)?1:symbol->array_size;
			mali_bool is_sampler = bs_symbol_is_sampler(symbol);
			mali_bool is_vertex = (symbol->location.vertex >= 0);
			mali_bool is_fragment= (symbol->location.fragment >= 0);

			loc->reg_location.vertex = is_vertex? vert_base + symbol->location.vertex : -1;
			loc->reg_location.fragment = is_fragment? frag_base + symbol->location.fragment : -1;
			loc->extra.sampler_location = is_sampler ? sampler_location++ : -1;

			loc->extra.base_index = 0;
			loc->symbol = symbol;

			/* do we have an actual array? */
			if(--arraysize)
			{
				s32 vert_loc = loc->reg_location.vertex;
				s32 frag_loc = loc->reg_location.fragment;
				s32 vert_inc = is_vertex?   symbol->array_element_stride.vertex   : 0;
				s32 frag_inc = is_fragment? symbol->array_element_stride.fragment : 0;
				s32 baseindex = 1;

				while (arraysize--)
				{
					loc++;
					n++;
					vert_loc += vert_inc;
					frag_loc += frag_inc;
					loc->reg_location.vertex = vert_loc;
					loc->reg_location.fragment = frag_loc;
					loc->extra.sampler_location = is_sampler? sampler_location++ : -1;
					loc->extra.base_index = baseindex++;
					loc->symbol = symbol;
				}
			}
			loc++;
			n++;

		} /* if datatype != struct */
	} /* for all symbols */

	/* next, repeat the process for all symbols */
	for(i = 0; i < table->member_count; i++)
	{
		struct bs_symbol* symbol = table->members[i];
		/* silently ignore null members */
		if(symbol == NULL) continue;

		if(symbol->datatype != DATATYPE_STRUCT) continue;

		/** attempt to filter out this symbol */
		if (bs_filter_symbol(symbol, filters, filtercount))
			continue;
		
		{
			u32 arraysize = (!symbol->array_size)?1:symbol->array_size;
			mali_bool is_vertex = (symbol->location.vertex >= 0);
			mali_bool is_fragment= (symbol->location.fragment >= 0);
			u32 c;

			s32 temp_vert_loc		= vert_base;
			s32 temp_frag_loc		= frag_base;
			s32 temp_vert_inc		= 0;
			s32 temp_frag_inc		= 0;
			if(is_vertex)   temp_vert_loc += symbol->location.vertex;
			if(is_fragment) temp_frag_loc += symbol->location.fragment;

			if(is_vertex) temp_vert_inc = symbol->array_element_stride.vertex;
			if(is_fragment) temp_frag_inc = symbol->array_element_stride.fragment;

			/* find one location per array count */
			for(c = 0; c < arraysize; c++)
			{
				u32 nplus;
				MALI_DEBUG_ASSERT(n < (u32) num_locations, ("Data write to uniform location table is out of bounds."));
				loc->reg_location.vertex	= temp_vert_loc;
				loc->reg_location.fragment	= temp_frag_loc;
				*psampler_count = sampler_location;

				nplus = recursive_bs_symbol_fill_location_table(&symbol->type.construct, num_locations, psampler_count, loc, filters, filtercount);
				loc += nplus;
				sampler_location = *psampler_count;
				n += nplus;
				temp_vert_loc += temp_vert_inc;
				temp_frag_loc += temp_frag_inc;

			}
		} /* if datatype == struct */
	} /* end for all symbols */

	*psampler_count = sampler_location;
	return n; /* total locations filled */
}

/* @brief recursive function that traverses the symbol table and fills the uniform location table.
 * @param table The table to look up in
 * @param num_locations Locations table size (how many locations it can house)
 * @param loc The location of an array of bs_uniform_location structures.
 * @param vertex_location, fragment_location The shader locations of the location found.
 * @param filters An array of namefilters to remove from the search! Each filter is basically a prefix.
 * @param filtercount The number of filters given
*/
MALI_EXPORT u32  bs_symbol_fill_location_table(struct bs_symbol_table* table, int num_locations, struct bs_uniform_location* loc, const char** filters, int filtercount)
{
	s32						psampler_count = 0;

	MALI_DEBUG_ASSERT(num_locations > 0,("Cannot fill a uniform location table of size zero"));
	
	/* initialize the locations */
	loc->reg_location.vertex	= 0;
	loc->reg_location.fragment	= 0;

	return recursive_bs_symbol_fill_location_table(table, num_locations, &psampler_count, loc, filters, filtercount);
}

/* wrapper, ensures the counter n works as intended */
MALI_EXPORT struct bs_symbol* bs_symbol_get_nth_location(struct bs_symbol_table* table, u32 n,
                                             struct bs_uniform_location* uniform_loc,
                                             const char** filters, int filtercount)
{
	struct bs_symbol* retval;

	/* simplify matters by always making the output variables valid */
	MALI_DEBUG_ASSERT_POINTER( uniform_loc );

	/* handle incomplete linking; we must still be able to query this information on unsuccessful linkage */
	if(table == NULL)
	{
		uniform_loc->reg_location.vertex = -1;
		uniform_loc->reg_location.fragment = -1;
		uniform_loc->extra.sampler_location = -1;
		return NULL; /* handle null tables */
	}

	/* initial recursion values */
	uniform_loc->reg_location.vertex = 0;
	uniform_loc->reg_location.fragment = 0;
	uniform_loc->extra.sampler_location = 0;
	uniform_loc->extra.base_index = 0;

	retval = wrap_bs_symbol_get_nth_location(table, &n, uniform_loc, filters, filtercount);

	if(retval == NULL)
	{
		uniform_loc->reg_location.vertex = -1;
		uniform_loc->reg_location.fragment = -1;
		uniform_loc->extra.sampler_location = -1;
		return NULL;
	}

	return retval;
}

/**
 * @brief Wrapper for the sampler enumeration function, used to easen the interface of the real function
 * @param table the table to look up in
 * @param n The numbered sampler we want, this number will decrement as we find samplers, once it hits zero we have our mark.
 * @param fragment_location output parameter The location of the sampelr we found
 * @param optimized output parameter, returns whether this sampler is optimized or not.
 * @return Returns the symbol of the sampler you ordered, or NULL if not found.
 */
MALI_STATIC struct bs_symbol* wrap_bs_symbol_get_nth_sampler(struct bs_symbol_table* table, u32* n, s32* fragment_location, mali_bool* optimized)
{
	u32 i;

	struct bs_symbol* retval = NULL;

	/* enum all samplers on this level - must be sorted by location in fragment shader! (auch!)*/
	for(i = 0; i < table->member_count; i++)
	{
		struct bs_symbol* symbol = table->members[i];
		u32 c, arraysize;

		if(!symbol) continue;  /* handle NULL-symbols (should not happen, but you never know) */
		if( !bs_symbol_is_sampler(symbol) ) continue; /* only check samplers in this pass */

		arraysize = (!symbol->array_size)?1:symbol->array_size;

		if(*fragment_location >= 0) *fragment_location += symbol->location.fragment;

		for( c = 0; c < arraysize; c++)
		{
			if(*fragment_location >= 0) *fragment_location += symbol->array_element_stride.fragment * c;

			if(*n == 0) return symbol;
			*n -= 1;

			if(*fragment_location >= 0) *fragment_location -= symbol->array_element_stride.fragment * c;
		}

		if(*fragment_location >= 0) *fragment_location -= symbol->location.fragment;
	}

	/* from this point on, no symbols are optimized (they are all struct members */
	*optimized = MALI_FALSE;

	/* enum all structs */
	for(i = 0; i < table->member_count; i++)
	{
		struct bs_symbol* symbol = table->members[i];
		u32 c, arraysize;

		if(!symbol) continue;  /* handle NULL-symbols (should not happen, but you never know) */
		if(symbol->datatype != DATATYPE_STRUCT) continue;
		if(symbol->location.fragment == -1) continue;

		if(*fragment_location >= 0) *fragment_location += symbol->location.fragment;

		arraysize = (!symbol->array_size)?1:symbol->array_size;
		for(c = 0; c < arraysize; c++)
		{
			if(*fragment_location >= 0) *fragment_location += symbol->array_element_stride.fragment * c;

			retval = wrap_bs_symbol_get_nth_sampler(&symbol->type.construct, n, fragment_location, optimized);
			if(retval) return retval;

			if(*fragment_location >= 0) *fragment_location -= symbol->array_element_stride.fragment * c;
		}

		if(*fragment_location >= 0) *fragment_location -= symbol->location.fragment;
	}

	return NULL;
}

/* wrapper, ensures the counter n works as intended */
MALI_EXPORT struct bs_symbol* bs_symbol_get_nth_sampler(struct bs_symbol_table* table, u32 n, s32* fragment_location, mali_bool* optimized)
{
	struct bs_symbol* retval;

	s32 fl;
	mali_bool opt = MALI_TRUE;

	if( NULL == fragment_location ) fragment_location = &fl;
	if( NULL == optimized ) optimized = &opt;

	if( NULL == table )
	{
		*fragment_location = -1;
		*optimized = MALI_FALSE;
		return NULL;
	}
	*fragment_location = 0;
	*optimized = MALI_TRUE;
	retval = wrap_bs_symbol_get_nth_sampler(table, &n, fragment_location, optimized);
	if( NULL == retval )
	{
		*fragment_location = -1;
		*optimized = MALI_FALSE;
	}
	return retval;
}

/**
 * @brief Wrapper for the active enumeration function. We use this wrapper to easen the interface of the real function (which is bad enough already!)
 * @param table the table to lookup for actives.
 * @param n The numbered active we want, this number will decrement as we find actives. Once it hits zero, we hvae hit our mark.
 * @param namebuffer Output param, contains the name of our active
 * @param namebuffersize Size of our buffer
 * @param filters An array of namefilters to remove from the search! Each filter is basically a prefix.
 * @param filtercount The number of filters given
 */
MALI_STATIC struct bs_symbol* wrap_bs_symbol_get_nth_active(struct bs_symbol_table* table, u32* n, char* namebuffer, u32 namebuffersize, const char** filters, int filtercount )
{
	struct bs_symbol* retval = NULL;
	u32 i, c;
	u32 namelen = 0;

	char nb[32];  /* a numberbuffer, will always be able to hold an u32 in text format, plus two brackets  */

	MALI_DEBUG_ASSERT_POINTER( table );

	/* Locate the location now */
	for(i = 0; i < table->member_count; i++)
	{
		struct bs_symbol* symbol = table->members[i];

		u32 arraysize = (!symbol->array_size)?1:symbol->array_size;
		u32 symbol_len = _mali_sys_strlen(symbol->name);
		u32 index_len = 0;

		/* silently ignore null members */
		if(table->members[i] == NULL) continue;

		/** attempt to filter out this symbol */
		if (bs_filter_symbol(symbol, filters, filtercount))
			continue;

		/* get the names and stuff */
		ADD_STRING_TO_BUFFER(symbol->name, symbol_len);

		if(symbol->datatype == DATATYPE_STRUCT)
		{
			/* find one location per array count */
			for(c = 0; c < arraysize; c++)
			{
				s32 sizeofdot = 1;
				/* do we have an actual array? */
				if(symbol->array_size)
				{
					_mali_sys_snprintf(nb, 32, "[%u]", (int) c);
					index_len = _mali_sys_strlen(nb);
					ADD_STRING_TO_BUFFER(nb, index_len);
				}

				ADD_STRING_TO_BUFFER(".", sizeofdot);
				retval = wrap_bs_symbol_get_nth_active(
									&symbol->type.construct, n,
									namebuffer+namelen, namebuffersize-namelen,
									filters, filtercount);
				if(retval)
				{
					return retval;  /* we found it! */
				}
				REMOVE_STRING_FROM_BUFFER(sizeofdot);

				/* undo any array stuff */
				if(symbol->array_size)
				{
					REMOVE_STRING_FROM_BUFFER(index_len);
				}
			}

		} else { /* not a struct */
			if(*n == 0) return symbol;
			*n -= 1;
		}
		REMOVE_STRING_FROM_BUFFER(symbol_len);
	}

	return retval;
}

/* wrapper, ensures that the counter n works as intended */
MALI_EXPORT struct bs_symbol* bs_symbol_get_nth_active(struct bs_symbol_table* table, u32 n, char* namebuffer, u32 namebuffersize, const char** filter, int filterlength )
{
	u32 nc = n;
	struct bs_symbol* retval;

	char fallback_namebuffer[1]; /* since the function require at least ONE byte of size, if the input name buffer is NULL, we redirect it to this one. */
	if(!namebuffer)
	{
		namebuffer = fallback_namebuffer;
		namebuffersize = 1;
	}

	if(table == NULL)
	{
		if(namebuffersize > 0) namebuffer[0] = '\0';
		return NULL; /* handle null tables */
	}
	retval = wrap_bs_symbol_get_nth_active(table, &nc, namebuffer, namebuffersize, filter, filterlength);

	if(retval == NULL)
	{
		if(namebuffersize > 0) namebuffer[0] = '\0';
	}
	return retval;
}

/********************************** MERGE SYMBOLS **********************************/

MALI_EXPORT void bs_symbol_merge_shadertype_specifics(struct bs_symbol* vertex, struct bs_symbol* fragment)
{
	/* all input params must be legal */
	MALI_DEBUG_ASSERT_POINTER( vertex);
	MALI_DEBUG_ASSERT_POINTER( fragment );

	/* assert that the symbols match in type; I won't bother to do a deep scan here as the driver should already have done that prior to usage. */
	MALI_DEBUG_ASSERT( vertex->datatype == fragment->datatype, ("Datatypes of symbols must match - this error should be handled earlier through bs_symbol_type_compare"));

	/* set location */
	fragment->location.vertex = vertex->location.vertex;
	vertex->location.fragment = fragment->location.fragment;

	/* set block stride */
	fragment->block_size.vertex = vertex->block_size.vertex;
	vertex->block_size.fragment = fragment->block_size.fragment;

	/* set array element stride */
	fragment->array_element_stride.vertex = vertex->array_element_stride.vertex;
	vertex->array_element_stride.fragment = fragment->array_element_stride.fragment;

	if( vertex->datatype == DATATYPE_STRUCT )
	{
		u32 i;
		MALI_DEBUG_ASSERT( vertex->type.construct.member_count == fragment->type.construct.member_count ,
					("struct size of symbols must match - this error should be handled earlier through bs_symbol_type_compare"));
		for(i = 0; i < vertex->type.construct.member_count; i++)
		{
			/* NOTE: this matching requires symbols to appear in the same order in the vshader and fshader - is this enough? */
			bs_symbol_merge_shadertype_specifics(vertex->type.construct.members[i], fragment->type.construct.members[i]);
		}
		return;
	}

	/* set vector stride */
	fragment->type.primary.vector_stride.vertex = vertex->type.primary.vector_stride.vertex;
	vertex->type.primary.vector_stride.fragment = fragment->type.primary.vector_stride.fragment;

	/* set essl precision */
	fragment->type.primary.essl_precision.vertex = vertex->type.primary.essl_precision.vertex;
	vertex->type.primary.essl_precision.fragment = fragment->type.primary.essl_precision.fragment;

	/* set bit precision */
	fragment->type.primary.bit_precision.vertex = vertex->type.primary.bit_precision.vertex;
	vertex->type.primary.bit_precision.fragment = fragment->type.primary.bit_precision.fragment;
}

/********************** COMPARE TYPE OF SYMBOLS ************************************/

MALI_EXPORT mali_bool bs_symbol_type_compare(struct bs_symbol* a, struct bs_symbol* b, char* buffer, u32 buffersize)
{
	/* handle NULL inputs */
	if(!a) return MALI_FALSE;
	if(!b) return MALI_FALSE;

	/* easy check - is the type identical? */
	if(a->datatype != b->datatype)
	{
		_mali_sys_snprintf(buffer, buffersize, "'%s' differ on type", a->name);
		return MALI_FALSE;
	}

	/* compare array size */
	if(a->array_size != b->array_size)
	{
		_mali_sys_snprintf(buffer, buffersize, "'%s' differ on array size", a->name);
		return MALI_FALSE;
	}

	/* do we have a struct? */
	if(a->datatype == DATATYPE_STRUCT)
	{
		u32 i;
		if(a->type.construct.member_count != b->type.construct.member_count)
		{
			_mali_sys_snprintf(buffer, buffersize, "'%s' struct member count mismatch", a->name);
			return MALI_FALSE;
		}

		for(i = 0; i < a->type.construct.member_count; i++)
		{
			/* due to lexigraphic sorting, the uniform symbols will appear in the same order in both shaders */
			mali_bool localmatch;
			localmatch = bs_symbol_type_compare(a->type.construct.members[i], b->type.construct.members[i], buffer, buffersize);
			if(localmatch == MALI_FALSE) return MALI_FALSE; /* buffer is already filled with error */
		}

		return MALI_TRUE;
	}

	/* check other sizes */
	if(a->type.primary.vector_size != b->type.primary.vector_size)
	{
		_mali_sys_snprintf(buffer, buffersize, "'%s' differ on type size", a->name);
		return MALI_FALSE;
	}

	/* compare invariance setting */
	if(a->type.primary.invariant != b->type.primary.invariant)
	{
		_mali_sys_snprintf(buffer, buffersize, "'%s' differ on invariance", a->name);
		return MALI_FALSE;
	}

	/*That's it, we have a match! */
	return MALI_TRUE;
}

MALI_EXPORT mali_bool bs_symbol_precision_compare(struct bs_symbol* vs, struct bs_symbol* fs, char* buffer, u32 buffersize)
{
	/* handle NULL inputs */
	if(!vs) return MALI_FALSE;
	if(!fs) return MALI_FALSE;

	/* do we have a struct? */
	if(vs->datatype == DATATYPE_STRUCT)
	{
		u32 i;
		if(vs->type.construct.member_count != fs->type.construct.member_count)
		{
			_mali_sys_snprintf(buffer, buffersize, "'%s' struct member count mismatch", vs->name);
			return MALI_FALSE;
		}

		for(i = 0; i < vs->type.construct.member_count; i++)
		{
			/* NOTE: this matching requires symbols to appear in the same order in the vshader and fshader - is this enough? */
			mali_bool localmatch;
			localmatch = bs_symbol_precision_compare(vs->type.construct.members[i], fs->type.construct.members[i], buffer, buffersize);
			if(localmatch == MALI_FALSE) return MALI_FALSE; /* buffer is already filled with error */
		}

		return MALI_TRUE;
	}

	/* check precision */
	if(vs->type.primary.essl_precision.vertex != fs->type.primary.essl_precision.fragment)
	{
		_mali_sys_snprintf(buffer, buffersize, "'%s' differ on precision", vs->name);
		return MALI_FALSE;
	}

	/*That's it, we have a match! */
	return MALI_TRUE;
}

