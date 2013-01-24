

// Copyright 2010 by Philipp Kraft
// This file is part of cmf.
//
//   cmf is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, either version 2 of the License, or
//   (at your option) any later version.
//
//   cmf is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with cmf.  If not, see <http://www.gnu.org/licenses/>.
//   
#ifndef ShuttleworthWallace_h__
#define ShuttleworthWallace_h__

#include "../cell.h"
#include <cmath>
#include "../../atmosphere/meteorology.h"
#include "ET.h"

namespace cmf {
	namespace upslope {
		namespace ET {
			/// Calculates the sum of soil evaporation and transpiration according to Shuttleworth & Wallace 1985, as implemented in BROOK 90 (Federer 1990)
			///
			/// The difference to BROOK90 is, that the actual transpiration is not calculated by plant resitance and potential gradient between plant and soil,
			/// but by an piecewise linear function of the pF value \f$ pF = \log_{10}\left(-\Psi [hPa]\right) \f$:
			/// \f[ \frac{T_{act}}{T_{pot}} = \left\{\begin{array}{cl}
			/// 1 & \mbox{if $pF \le 3.35$} \\ 
			/// \frac{pF - 4.2}{3.35 - 4.2} & \mbox{if $pF \in [3.35 .. 4.2] $} \\
			/// 0 & \mbox{if $pF \ge 4.2$} \end{array}\right. \f]
			///
			/// Calculation procedure, as in BROOK 90:
			///
			/// Evapotranspiration from the canopy: 
			/// \f$\lambda ET_{canopy} = \frac {r_{ac} \Delta\ R_{n,canopy} + c_p\rho D_0}{\Delta \gamma r_{ac} + \gamma r_{sc}} \f$
			///
			/// Evaporation from the ground: 
			/// \f$\lambda E_{ground} = \frac {r_{as} \Delta\ R_{n,ground} + c_p\rho D_0}{\Delta \gamma r_{as} + \gamma r_{ss}} \f$
			///
			/// with
			///  - \f$ \Delta = \frac{de_s}{dT} = 4098\ 0.6108 \exp\left(\frac{17.27 T}{T+237.3}\right)(T+237.3)^{-2} \f$, the slope of the sat. vap. press. T function
			///  - \f$ R_{n,ground} = R_n \exp(-C_R LAI) \f$, the net radiation flux in the ground
			///  - \f$ R_{n_canopy} = R_n - R_{n,ground} \f$, the net radiation flux in the canopy
			///  - \f$ \lambda,c_p\rho,\gamma,C_R \f$ constants lambda, c_p_rho, gamma, C_R
			///  - \f$ D_0 \f$ vapor pressure deficit at effective source height, see function D0
			///  - \f$ r_{ac}, r_{sc}, r_{as}, r_{ss} \f$ Resistances for the vapor pressure (see below)
			///
			/// @todo Include surface water below canopy, eg. for rice paddies.

			class ShuttleworthWallace 
				: public transpiration_method, 
				  public soil_evaporation_method, 
				  public surface_water_evaporation_method, 
				  public canopy_evaporation_method,
				  public snow_evaporation_method,
				  public cmf::atmosphere::aerodynamic_resistance
			{
			private:
				cmf::upslope::Cell& cell;
				mutable cmf::math::Time refresh_time;
				static double RSSa,RSSb,RSSa_pot;
			public:
				double RAA,RAC,RAS,RSS,RSC;
				int refresh_counter;
				/// Calculates all the values
				void refresh(cmf::math::Time t);
				void refresh() {
					refresh_time-=cmf::math::min;
					refresh(refresh_time+cmf::math::min);
				}
				/// Potential transpiration rate in mm/day
				double PTR;
				/// Potential snow evaporation rate in mm/day
				double PSNVP;
				double ASNVP;
				/// Ground evaporation rate in mm/day
				double GER; 
				/// Potential leaf evaporation rate in mm/day
				double PIR;
				/// Actual leaf evaporation rate in mm/day
				double AIR;
				/// surface water evaporation rate in mm/day
				double GIR; 
				/// actual transpiration rate in mm/day
				double ATR_sum;
				/// actual transpiration rate per layer in mm/day
				cmf::math::num_array ATR;
				/// multiplier to reduce snow evaporation, dimensionless. 
				/// KSNVP is an arbitrary and frustrating correction factor 
				/// needed for snow evaporation (SNVP) to substantially reduce its value. 
				/// This is the only arbitrary fudge factor in BROOK90 and is needed 
				/// because the Shuttleworth-Gurney aerodynamic resistances as corrected for SAI 
				/// are much too low to give reasonable snow evaporation from forests
				double KSNVP;


				virtual double transp_from_layer(cmf::upslope::SoilLayer::ptr sl,cmf::math::Time t) {
					if (sl->RecalcFluxes(t)) refresh(t);
					if (sl->Position>ATR.size()) return 0.0;
					return ATR[sl->Position] * 1e-3 * cell.get_area();
				}
				virtual double evap_from_layer(cmf::upslope::SoilLayer::ptr sl,cmf::math::Time t) {
					// Evaporation only from the first layer
					if (sl->Position != 0) return 0.0;
					// Recalculate fluxes if needed	
					if (sl->RecalcFluxes(t)) refresh(t);
					// Return soil evap. for snow free area
					return GER * std::max(0.0,1 - cell.snow_coverage() - cell.surface_water_coverage())   * 1e-3 * cell.get_area();
				}
				virtual double evap_from_openwater(cmf::river::OpenWaterStorage::ptr ows,cmf::math::Time t) {
					// If open water is empty return zero
					if (ows->RecalcFluxes(t)) refresh(t);
					return GIR * ows->get_height_function().A(ows->get_volume()) * 1e-3 * (1-ows->is_empty());
				}
				virtual double evap_from_canopy(cmf::water::WaterStorage::ptr canopy,cmf::math::Time t) {
					if (canopy->RecalcFluxes(t)) refresh(t);
					return AIR * 1e-3 * cell.get_area() * (1-canopy->is_empty());
				}
				virtual double evap_from_snow(cmf::water::WaterStorage::ptr snow,cmf::math::Time t) {
					if (snow->RecalcFluxes(t)) refresh(t);
					return ASNVP * 1e-3 * cell.get_area() * (1-snow->is_empty());
				}

				virtual void get_aerodynamic_resistance(double & r_ag,double & r_ac, cmf::math::Time t)  const{
					r_ag = RAA + RAS;
					r_ac = RAA + RAC;
				}

				/// Sets the parameters of the soil surface resistance, a function of the actual water potential
				/// \f[ r^s_s = RSS_a \left(\frac{\Psi}{\Psi_{RSS_a}}\right)^RSS_b \f]
				/// @param _RSSa Resistance in s/m at potential in RSSa_pot
				/// @param _RSSb Exponent for curve shape
				/// @param _RSSa_pot Matrix potential (m), where \f$r^s_s = RSS_a\f$
				static void set_RSS_parameters(double _RSSa=500., double _RSSb=1.0, double _RSSa_pot=-3.22) {
					RSSa = _RSSa;
					RSSb = _RSSb;
					RSSa_pot = _RSSa_pot;
				}




				/// Calculates the transpiration and the soil evaporation from dry surfaces
				ShuttleworthWallace(cmf::upslope::Cell& cell);

				static ShuttleworthWallace* use_for_cell(cmf::upslope::Cell& cell);
			};
		}		
	}
	
}
#endif // ShuttleworthWallace_h__
