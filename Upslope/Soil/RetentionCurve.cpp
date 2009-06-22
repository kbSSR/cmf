#include "RetentionCurve.h"
#include <cmath>
#include "../../math/real.h"

/// Converts a pressure in Pa to a length of a water column in m
double cmf::upslope::pressure_to_waterhead(double Pressure)		{	return Pressure/rho_wg;		}
/// Converts a height of a water column in m to a pressure in Pa
double cmf::upslope::waterhead_to_pressure(double waterhead)  { return waterhead*rho_wg;  }
/// Converts a pF value to a height of a water column in m
double cmf::upslope::pF_to_waterhead(double pF)               { return -pow(10,pF+2)/rho_wg;}
/// Converts a height of a water column to a pF value
double cmf::upslope::waterhead_to_pF(double waterhead)        { return log10(fabs(waterhead*rho_wg))-2;}

real cmf::upslope::BrooksCoreyRetentionCurve::K( real wetness,real depth ) const
{
	real Ksat=m_Ksat*pow(1-m_KsatDecay,depth);
	return Ksat*minimum(pow(wetness,2+3*b()),1);
}

real cmf::upslope::BrooksCoreyRetentionCurve::Porosity( real depth ) const
{
	return m_Porosity*pow(1-m_PorosityDecay,depth);
}

void cmf::upslope::BrooksCoreyRetentionCurve::SetPorosity( real porosity,real porosity_decay/*=0*/ )
{
	m_Porosity = porosity;m_PorosityDecay=porosity_decay;
}

real cmf::upslope::BrooksCoreyRetentionCurve::VoidVolume( real upperDepth,real lowerDepth,real Area ) const
{
	if (m_PorosityDecay==0)
		return (lowerDepth-upperDepth)*Area*Porosity(upperDepth);
	else
	{
		real C=-log(1-m_PorosityDecay); // integration constant
		return (Porosity(upperDepth) - Porosity(lowerDepth)) * Area/C;
	}
}

real cmf::upslope::BrooksCoreyRetentionCurve::FillHeight( real lowerDepth,real Area,real Volume ) const
{
	if (m_PorosityDecay==0) 
		return Volume/(Porosity(lowerDepth)*Area);
	else
	{
		real V_to_lower=VoidVolume(0,lowerDepth,Area);	 // Volume from surface to lower depth
		real V_to_upper=V_to_lower - Volume;						 // Volume from surface to unknown upper depth
		real C=log(1-m_PorosityDecay);									 // integration constant
		// The function here is the VoidVolume function solved for depth
		real d_upper=(log(C*V_to_upper+m_Porosity)-log(m_Porosity))/C;	// upper depth
		return lowerDepth-d_upper; // return fill height
			
	}
}

real cmf::upslope::BrooksCoreyRetentionCurve::Transmissivity( real upperDepth,real lowerDepth,real wetness) const
{
	if (upperDepth>=lowerDepth) return 0.0;
	if (m_KsatDecay==0)
		return (lowerDepth-upperDepth)*K(wetness,lowerDepth);
	else
	{
		real C=-log(1-m_KsatDecay);	// integration constant
		return (K(wetness,upperDepth) - K(wetness,lowerDepth))/C;
	}
}

real cmf::upslope::BrooksCoreyRetentionCurve::MatricPotential( real wetness ) const
{
	real linear_part=10*(wetness-1.0);
	real f_Transition=pow(piecewise_linear(wetness,.98,1.0),10);

	real bc_part=wetness<1 ? Psi_X * pow(wetness/wetness_X,-b()) : 0;
	return (1-f_Transition)*bc_part+f_Transition*linear_part;

}
real cmf::upslope::BrooksCoreyRetentionCurve::Wetness( real suction ) const
{
	real linear_part=(suction/10)+1;
	real bc_part=1;
	if (suction<0)
	{
		if (suction>=m_Psi_i)
		{
			real Psi=suction,
				m=m_m,
				n=m_n,
				m2=m_m*m_m,
				n2=m_n*m_n;
			bc_part=(sqrt(4*m*Psi+m2*n2-2*m2*n+m2)+m*n+m)/(2*m);
		}
		else
			bc_part= pow(Psi_X/suction,1/m_b)*wetness_X;
	}
	real f_Transition=pow(piecewise_linear(bc_part,.98,1.0),10);
	return (1-f_Transition)*bc_part+f_Transition*linear_part;

}


cmf::upslope::BrooksCoreyRetentionCurve::BrooksCoreyRetentionCurve( real ksat/*=15*/,real porosity/*=0.5*/,real _b/*=5*/,real theta_x/*=0.2*/,real psi_x/*=pF_to_waterhead(2.5)*/,real ksat_decay/*=0*/,real porosity_decay/*=0*/ ) : m_Ksat(ksat),m_Porosity(porosity),m_b(_b),wetness_X(theta_x/porosity),Psi_X(psi_x),m_PorosityDecay(porosity_decay),m_KsatDecay(ksat_decay)
{
	Set_Saturated_pF_curve_tail_parameters();
}

cmf::upslope::BrooksCoreyRetentionCurve cmf::upslope::BrooksCoreyRetentionCurve::CreateFrom2Points( real ksat,real porosity,real theta1,real theta2,real psi_1/*=pF_to_waterhead(2.5)*/,real psi_2/*=pF_to_waterhead(4.2)*/ )
{
	return BrooksCoreyRetentionCurve(ksat,porosity,log(psi_1/psi_2)/log(theta2/theta1),theta1,psi_1);
}

void cmf::upslope::BrooksCoreyRetentionCurve::Set_Saturated_pF_curve_tail_parameters()
{
	m_Wi=0.9+0.005*m_b;
	m_Psi_i=Psi_X*pow(m_Wi/wetness_X,-m_b);
	m_m=-m_Psi_i/((1-m_Wi)*(1-m_Wi))-m_b*(-m_Psi_i)/(m_Wi*(1-m_Wi));
	m_n=2*m_Wi-(m_b*(-m_Psi_i))/(m_m*m_Wi)-1;
}



real cmf::upslope::VanGenuchtenMualem::Wetness( real suction) const
{
	real m=1-1/n;
	real h=-100 * suction;
	real h0=-100 * Psi_full;
	real w=pow(1+pow(alpha*maximum(h,h0),n),-m);
	if (suction>Psi_full)
	{
		real w0=Wetness(Psi_full);
		// Slope dw/dh at h0 
		real s=-m*n*pow(1 + pow(alpha*h0,n),-m)*pow(alpha*h0,n)/(h0*(1 + pow(alpha*h0,n)));
		real w_max=(square(w0)-w0-h0*s)/(w0-h0*s-1);
		// Add Michaelis Menten extrapolation
		w+= (w_max-w) * (h-h0)
							/((w_max-w)/s + (h-h0));
	}
	return w;
}
	

real cmf::upslope::VanGenuchtenMualem::MatricPotential( real w ) const
{
	real w0=Wetness(Psi_full); // Wetness at the border between non-linear and linear part = wetness(h0)
	if (w<w0)
	{
		// inverse m and inverse n
		real mi=1/(1-1/n),ni=1/n;

		return -0.01*pow(1-pow(w,mi),ni)/(alpha*pow(w,mi*ni));
	}
	else
	{
		real m=1-1/n;
		// Pressure head at the border between non-linear and linear part (in cm)
		real h0=-100 * Psi_full; 
		// Slope dw/dh(h0)
		real s=-m*n*pow(1 + pow(alpha*h0,n),-m)*pow(alpha*h0,n)/(h0*(1 + pow(alpha*h0,n)));
		// Maximum wetness. Solution of eq.: 1 = w0 + (0-h0)*(w_max-w0)/((w_max-w0)/s + (0 - h0))
		real w_max=(square(w0)-w0-h0*s)/(w0-h0*s-1);
		// wetness difference
		real w_diff=w_max-w0;
		// michaelis menten k 
		real k_m=-w_diff/s;
		// inverse Michaelis-Menten extrapolation
		real h_diff = (w0-w)*k_m/(w_diff+w0-w);
		return -0.01 * (h0 + h_diff);
	}
}

real cmf::upslope::VanGenuchtenMualem::K( real wetness,real depth ) const
{
	if (wetness>=1) return Ksat*exp(-(wetness-1)*10);
	real m=1-1/n;
	return Ksat*minimum(sqrt(wetness)*square(1-pow(1-pow(wetness,1/m),m)),1);
}

real cmf::upslope::VanGenuchtenMualem::VoidVolume( real upperDepth,real lowerDepth,real Area ) const
{
	return (lowerDepth-upperDepth)*Area*Porosity(upperDepth);
}

real cmf::upslope::VanGenuchtenMualem::Transmissivity( real upperDepth,real lowerDepth,real wetness ) const
{
	return K(wetness,lowerDepth)*(lowerDepth-upperDepth);
}

real cmf::upslope::VanGenuchtenMualem::Porosity( real depth ) const
{
	return Phi;
}

real cmf::upslope::VanGenuchtenMualem::FillHeight( real lowerDepth,real Area,real Volume ) const
{
	return Volume/(Area*Phi);
}

cmf::upslope::VanGenuchtenMualem* cmf::upslope::VanGenuchtenMualem::copy() const
{
	return new VanGenuchtenMualem(*this);
}

