

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
#ifndef cell_h__
#define cell_h__
#include "../atmosphere/meteorology.h"
#include "../atmosphere/precipitation.h"
#include "../geometry/geometry.h"
#include "../water/flux_connection.h"
#include "vegetation/StructVegetation.h"
#include "../water/WaterStorage.h"
#include "../math/statevariable.h"
#include "Soil/RetentionCurve.h"
#include "SoilLayer.h"
#include "layer_list.h"
#include <memory>
#include <map>
#include <vector>
#include <set>
namespace cmf {
	class project;
	namespace river {
		class OpenWaterStorage;	
		class Reach;
	}
	namespace atmosphere {
		class RainCloud;


	}
    /// Contains the classes to describe the discretization of the soil continuum
	/// @todo: Get Id in constructor for better naming of bounday conditions
	namespace upslope {
		class Topology;
		class Cell;
		typedef void (*connectorfunction)(cmf::upslope::Cell&,cmf::upslope::Cell&,int);
		typedef void (*internal_connector)(cmf::upslope::Cell&);
		typedef std::tr1::shared_ptr<SoilLayer> layer_ptr;
		/// A helper class to connect cells with flux_connection objects. This is generated by flux_connection classes, intended to connect cells 
		class CellConnector
		{
		private:
			connectorfunction m_connector;
			CellConnector():m_connector(0) {}
		public:
			CellConnector(connectorfunction connector) : m_connector(connector)			{	}
			void operator()(cmf::upslope::Cell& cell1,cmf::upslope::Cell& cell2,int start_at_layer=0) const	{
				connect(cell1,cell2,start_at_layer);
			}
			void connect(cmf::upslope::Cell& cell1,cmf::upslope::Cell& cell2,int start_at_layer=0) const {
				m_connector(cell1,cell2,start_at_layer);
			}
		};


		/// This class is the basic landscape object. It is the owner of water storages, and the upper and lower boundary conditions 
		/// of the system (rainfall, atmospheric vapor, deep groundwater)
		class Cell : public cmf::math::StateVariableOwner {
			Cell(const Cell& cpy);
			friend class project;
			/// @name Location
			//@{
			std::auto_ptr<Topology> m_topo;
			static int cell_count;
		public:
			cmf::upslope::Topology& get_topology();
#ifndef SWIG
			operator cmf::upslope::Topology&() {return get_topology();}
#endif
			double x,y,z;
			/// Returns the location of the cell
			cmf::geometry::point get_position() const { return cmf::geometry::point(x,y,z); }
		private:
			double m_Area;
			mutable real m_SatDepth;
		public:
			/// Returns the area of the cell
			double get_area() const { return m_Area; }
			/// Converts a volume in m3 in mm for the cell area
			double m3_to_mm(double volume) const { return volume/m_Area * 1e3;}
			double mm_to_m3(double depth) const { return depth * m_Area * 1e-3;}
			/// @name Saturation
			//@{
			/// Marks the saturated depth as unvalid
			void InvalidateSatDepth() const {m_SatDepth=-1e20;}
			/// 
			real get_saturated_depth() const;
			void set_saturated_depth(real depth);
			//@}
		private:
			//@}
			/// @name Flux nodes of the cell
			//@{
			typedef std::vector<cmf::water::WaterStorage::ptr> storage_vector;
			typedef std::auto_ptr<cmf::atmosphere::Meteorology> meteo_pointer;
			storage_vector m_storages;
			cmf::water::WaterStorage::ptr
				m_Canopy,
				m_Snow,
				m_SurfaceWaterStorage;
			cmf::water::flux_node::ptr m_SurfaceWater;
			cmf::atmosphere::RainSource::ptr m_rainfall;
			cmf::water::flux_node::ptr m_Evaporation, m_Transpiration;
			cmf::atmosphere::aerodynamic_resistance::ptr m_aerodynamic_resistance;

			const cmf::project & m_project;
			meteo_pointer m_meteo;

			
		public:
			cmf::upslope::vegetation::Vegetation vegetation;
			/// Returns the meteorological data source
			cmf::atmosphere::Meteorology& get_meteorology() const 
			{
				return *m_meteo;
			}
			/// Sets the method to calculate aerodynamic resistance against turbulent sensible heat fluxes
			void set_aerodynamic_resistance(cmf::atmosphere::aerodynamic_resistance::ptr Ra) {
				m_aerodynamic_resistance = Ra;
			}
			/// Sets a meteorological data source
			void set_meteorology(const cmf::atmosphere::Meteorology& new_meteo)
			{
				m_meteo.reset(new_meteo.copy());
			}
			/// Sets the weather for this cell. Connectivity to a meteorological station is lost.
			void set_weather(const cmf::atmosphere::Weather& weather)
			{
				cmf::atmosphere::ConstantMeteorology meteo(weather);
				m_meteo.reset(meteo.copy());
			}
			/// Exchanges a timeseries of rainfall with a constant flux
			void set_rainfall(double rainfall);
			/// Returns the current rainfall flux in m3/day
			double get_rainfall(cmf::math::Time t) const {return m_rainfall->get_intensity(t) * 1e-3 * m_Area;}
			/// Changes the current source of rainfall
			void set_rain_source(cmf::atmosphere::RainSource::ptr new_source);
			/// Returns the current source for rainfall
			cmf::atmosphere::RainSource::ptr get_rain_source() {
				return m_rainfall;
			}
			/// Returns the end point of all evaporation of this cell
 			cmf::water::flux_node::ptr get_evaporation();
 			/// Returns the end point of all transpiration of this cell
 			cmf::water::flux_node::ptr get_transpiration();
			/// returns the surface water of this cell
			cmf::water::flux_node::ptr get_surfacewater();
			void surfacewater_as_storage();
			cmf::water::WaterStorage::ptr add_storage(std::string Name,char storage_role='N',  bool isopenwater=false);
			void remove_storage(cmf::water::WaterStorage& storage);
			int storage_count() const
			{
				return int(m_storages.size());
			}
			cmf::water::WaterStorage::ptr get_storage(int index) const;
			cmf::water::WaterStorage::ptr get_canopy() const;
			cmf::water::WaterStorage::ptr get_snow() const;
			real snow_coverage() const;
			real albedo() const;
			real surface_amplitude;
			/// Returns the coverage of the surface water.
			///
			/// The covered fraction (0..1) is simply modelled as a piecewise linear
			/// function of the surface water depth. If the depth is above the
			/// aggregate height, the coverage is 1, below it is given as
			/// \f[ c = \frac{h_{water}}{\Delta h_{surface}}\f]
			/// with c the coverage, \f$h_{water}\f$ the depth of the surface water and
			/// \f$\Delta h_{surface}\f$ the amplitude of the surface roughness
			real surface_water_coverage() const;
			/// Calculates the surface heat balance
			/// @param t Time step
			real heat_flux(cmf::math::Time t) const;
			real Tground;
			bool has_wet_leaves() const
			{
				return (m_Canopy) && (m_Canopy->get_state()>1e-6*get_area());
			}
			bool has_surface_water() const
			{
				return (m_SurfaceWaterStorage) && (m_SurfaceWaterStorage->get_state()>1e-6*get_area());
			}
			int Id;
			const cmf::project& get_project() const;
			cmf::atmosphere::Weather get_weather(cmf::math::Time t) const;

		public:
			//@}
			///@name Layers
			//@{
		private:
			//typedef std::vector<layer_ptr> layer_vector;
			layer_list m_Layers;
		public:
			int layer_count() const
			{
				return int(m_Layers.size());
			}
			cmf::upslope::SoilLayer::ptr get_layer(int ndx) const;
			const layer_list& get_layers() const {return m_Layers;}
			void add_layer(real lowerboundary,const cmf::upslope::RetentionCurve& r_curve,real saturateddepth=10);
			void remove_last_layer();
			void remove_layers();
			double get_soildepth() const;

			/// Returns the flux to each layer from the upperlayer, or, in case of the first layer from the surface water 
			cmf::math::num_array get_percolation(cmf::math::Time t) const;


			virtual ~Cell();
			//@}


			Cell(double x,double y,double z,double area,cmf::project & _project);
			std::string to_string() const;
			//@}
			cmf::math::StateVariableList get_states();
		};

		
	}
	
}
#endif // cell_h__
