//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include <halton.h>

HaltonSequenceGenerator_t::HaltonSequenceGenerator_t(int b)
{
	// NOTE: base must be prime and >= 2. A base of 0 or 1 makes GetElement() divide by
	// zero (0) or loop forever, because tmpseed /= 1 never decreases the accumulator.
	base = ( b >= 2 ) ? b : 2;
	fbase=(float) base;
	seed=1;

}

float HaltonSequenceGenerator_t::GetElement(int elem)
{
	// NOTE: the loop below used to start from the current "seed" (the generator's
	// internal counter) and completely ignored the elem argument, so asking for
	// element N returned whatever the next value happened to be.
	int tmpseed = elem;
	float ret=0.0;
	float base_inv=1.0/fbase;
	while(tmpseed)
	{
		int dig=tmpseed % base;
		ret+=((float) dig)*base_inv;
		base_inv/=fbase;
		tmpseed/=base;
	}
	return ret;
}
