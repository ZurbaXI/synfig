/* === S Y N F I G ========================================================= */
/*!	\file valuenode_dynamic.cpp
**	\brief Implementation of the "Dynamic" valuenode conversion.
**
**	$Id$
**
**	\legal
**	Copyright (c) 2014 Carlos López
**
**	This package is free software; you can redistribute it and/or
**	modify it under the terms of the GNU General Public License as
**	published by the Free Software Foundation; either version 2 of
**	the License, or (at your option) any later version.
**
**	This package is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**	General Public License for more details.
**	\endlegal
*/
/* ========================================================================= */

/* === H E A D E R S ======================================================= */

#ifdef USING_PCH
#	include "pch.h"
#else
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "valuenode_dynamic.h"
#include "valuenode_const.h"
#include "general.h"
#include <ETL/misc>

#include <boost/numeric/odeint/integrate/integrate.hpp>
#endif

/* === U S I N G =========================================================== */

using namespace std;
using namespace etl;
using namespace synfig;
using namespace boost::numeric::odeint;
/* === M A C R O S ========================================================= */

/* === G L O B A L S ======================================================= */


/* === P R O C E D U R E S ================================================= */

/* === M E T H O D S ======================================================= */

ValueNode_Dynamic::ValueNode_Dynamic(const ValueBase &value):
	LinkableValueNode(value.get_type())
{
	Vocab ret(get_children_vocab());
	set_children_vocab(ret);
	set_link("origin",       ValueNode_Const::create(Vector(0,0)));
	set_link("force",        ValueNode_Const::create(Vector(0,0)));
	set_link("damping",      ValueNode_Const::create(Real(0.1)));
	set_link("friction",     ValueNode_Const::create(Real(0.1)));
	set_link("spring",       ValueNode_Const::create(Real(1.0)));
	set_link("torsion",      ValueNode_Const::create(Real(1.0)));
	set_link("mass",         ValueNode_Const::create(Real(1.0)));
	set_link("inertia",      ValueNode_Const::create(Real(1.0)));


	switch(get_type())
	{
	case ValueBase::TYPE_VECTOR:
		set_link("tip_static",ValueNode_Const::create(value.get(Vector())));
		break;
	default:
		throw Exception::BadType(ValueBase::type_local_name(get_type()));
	}

	/* Initial values*/
	state.resize(4);
	reset_state(Time(0.0));

	/*Derivative of the base position*/
	origin_d_=ValueNode_Derivative::create(ValueBase(Vector()));

	/* Initialize the last time called to be 0*/
	last_time=Time(0);
}

void
ValueNode_Dynamic::reset_state(Time t)const
{
	state[0]=((*tip_static_)(t).get(Vector())-(*origin_)(t).get(Vector())).mag();
	state[1]=0.0; // d/dt(radius) = 0 initially
	state[2]=(double)((Angle::rad(((*tip_static_)(t).get(Vector())-(*origin_)(t).get(Vector())).angle())).get());
	state[3]=0.0; // d/dt(angle) = 0 initially
}
LinkableValueNode*
ValueNode_Dynamic::create_new()const
{
	return new ValueNode_Dynamic(get_type());
}

ValueNode_Dynamic*
ValueNode_Dynamic::create(const ValueBase &x)
{
	return new ValueNode_Dynamic(x);
}

ValueNode_Dynamic::~ValueNode_Dynamic()
{
	unlink_all();
}

ValueBase
ValueNode_Dynamic::operator()(Time t)const
{
	if (getenv("SYNFIG_DEBUG_VALUENODE_OPERATORS"))
		printf("%s:%d operator()\n", __FILE__, __LINE__);
	double t0=last_time;
	double t1=t;
	double step;
	// If we are at the initial conditions
	if(t1==t0 && t0==0.0)
	{
		reset_state(Time(0.0));
		return (*origin_)(0).get(Vector()) + Vector(state[0], Angle::rad(state[2]));
	}
	// If we are playing backwards then calculate the value from the initial conditions
	if(t1<t0 && t0>0.0)
	{
		reset_state(Time(0.0));
		last_time=Time(0);
		t0=0.0;
	}
	// Prepare the step size based on distance between start and end time
	step=(t1-t0)/4.0;
	// Before call the integrator we need to be sure that the derivative of the
	// origin is properly set. Maybe the user changed the origin
	ValueNode::RHandle value_node(ValueNode::RHandle::cast_dynamic(origin_d_->get_link("link")));
	value_node->replace(origin_);
	Oscillator oscillator(this);
	std::vector<double> x(state.begin(), state.end());
	synfig::info("x.angle speed  = %f", x[3]);
	size_t steps = integrate(oscillator, x, t0, t1, step);

	synfig::info("Integration in %d steps", steps);

	// Remember time and state for the next call
	last_time=Time(t);
	state.assign(x.begin(), x.end());

	return (*origin_)(t).get(Vector()) + Vector(state[0], Angle::rad(state[2]));
}


String
ValueNode_Dynamic::get_name()const
{
	return "dynamic";
}

String
ValueNode_Dynamic::get_local_name()const
{
	return _("Dynamic");
}

bool
ValueNode_Dynamic::check_type(ValueBase::Type type)
{
	return
		type==ValueBase::TYPE_VECTOR	;
}

bool
ValueNode_Dynamic::set_link_vfunc(int i,ValueNode::Handle value)
{
	assert(i>=0 && i<link_count());

	switch(i)
	{
	case 0: CHECK_TYPE_AND_SET_VALUE(tip_static_,    get_type());
	case 1: CHECK_TYPE_AND_SET_VALUE(origin_,        ValueBase::TYPE_VECTOR);
	case 2: CHECK_TYPE_AND_SET_VALUE(force_,         ValueBase::TYPE_VECTOR);
	case 3: CHECK_TYPE_AND_SET_VALUE(damping_coef_,  ValueBase::TYPE_REAL);
	case 4: CHECK_TYPE_AND_SET_VALUE(friction_coef_, ValueBase::TYPE_REAL);
	case 5: CHECK_TYPE_AND_SET_VALUE(spring_coef_,   ValueBase::TYPE_REAL);
	case 6: CHECK_TYPE_AND_SET_VALUE(torsion_coef_,  ValueBase::TYPE_REAL);
	case 7: CHECK_TYPE_AND_SET_VALUE(mass_,          ValueBase::TYPE_REAL);
	case 8: CHECK_TYPE_AND_SET_VALUE(inertia_,       ValueBase::TYPE_REAL);
	}
	return false;
}

ValueNode::LooseHandle
ValueNode_Dynamic::get_link_vfunc(int i)const
{
	assert(i>=0 && i<link_count());

	switch(i)
	{
	case 0: return tip_static_;
	case 1: return origin_;
	case 2: return force_;
	case 3: return damping_coef_;
	case 4: return friction_coef_;
	case 5: return spring_coef_;
	case 6: return torsion_coef_;
	case 7: return mass_;
	case 8: return inertia_;
	default:
		return 0;
	}
}

LinkableValueNode::Vocab
ValueNode_Dynamic::get_children_vocab_vfunc()const
{
	if(children_vocab.size())
		return children_vocab;

	LinkableValueNode::Vocab ret;
	ret.push_back(ParamDesc(ValueBase(),"tip_static")
		.set_local_name(_("Tip static"))
		.set_description(_("Equilibrium tip position without external forces"))
	);
	ret.push_back(ParamDesc(ValueBase(),"origin")
		.set_local_name(_("Origin"))
		.set_description(_("Basement of the dynamic system"))
	);
	ret.push_back(ParamDesc(ValueBase(),"force")
		.set_local_name(_("Force"))
		.set_description(_("External force applied on the mass center of gravity"))
	);
	ret.push_back(ParamDesc(ValueBase(),"damping")
		.set_local_name(_("Damping coeficient"))
		.set_description(_("Radial damping coeficient of the dynamic sytem"))
	);
	ret.push_back(ParamDesc(ValueBase(),"friction")
		.set_local_name(_("Friction coeficient"))
		.set_description(_("Rotational friction coeficient of the dynamic sytem"))
	);
	ret.push_back(ParamDesc(ValueBase(),"spring")
		.set_local_name(_("Spring coeficient"))
		.set_description(_("Radial spring coeficient of the dynamic system"))
	);
	ret.push_back(ParamDesc(ValueBase(),"torsion")
		.set_local_name(_("Torsion coeficient"))
		.set_description(_("Torsion coeficient of the dynamic system"))
	);
	ret.push_back(ParamDesc(ValueBase(),"mass")
		.set_local_name(_("Mass"))
		.set_description(_("Mass of the dynamic system"))
	);
	ret.push_back(ParamDesc(ValueBase(),"inertia")
		.set_local_name(_("Moment of inertia"))
		.set_description(_("Moment of inertia of the dynamic system"))
	);
	return ret;
}

