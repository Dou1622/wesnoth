/* $Id$ */
/*
   Copyright (C) 2003 - 2009 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef TERRAIN_FILTER_H_INCLUDED
#define TERRAIN_FILTER_H_INCLUDED

#include "map_location.hpp"
#include "pathfind.hpp"

class config;
class gamestatus;
class unit;
class vconfig;
class unit_map;
class tod_manager;
class team;

//terrain_filter: a class that implements the Standard Location Filter
class terrain_filter : public xy_pred {
public:
#ifdef _MSC_VER
	// This constructor is required for MSVC 9 SP1 due to a bug there
	// see http://social.msdn.microsoft.com/forums/en-US/vcgeneral/thread/34473b8c-0184-4750-a290-08558e4eda4e
	// other compilers don't need it.
	terrain_filter();
#endif
	terrain_filter(const vconfig& cfg, const gamemap& map, const gamestatus& game_status, const std::vector<team>& teams,
		const unit_map& units, const bool flat_tod=false, const size_t max_loop=MAX_MAP_AREA);
	terrain_filter(const vconfig& cfg, const terrain_filter& original);
	~terrain_filter() {};

	terrain_filter(const terrain_filter &other);
	terrain_filter& operator=(const terrain_filter &other);

	//match: returns true if and only if the given location matches this filter
	bool match(const map_location& loc);
	virtual bool operator()(const map_location& loc) { return this->match(loc); }

	//get_locations: gets all locations on the map that match this filter
	// @param locs - out parameter containing the results
	void get_locations(std::set<map_location>& locs);

	//restrict: limits the allowed radius size and also limits nesting
	// The purpose to limit the time spent for WML handling
	// Note: this feature is not fully implemented, e.g. SLF inside SUF inside SLF
	void restrict_size(const size_t max_loop) { max_loop_ = max_loop; }

	//flatten: use base time of day -- ignore illumination ability
	void flatten(const bool flat_tod=true) { flat_ = flat_tod; }

private:
	bool match_internal(const map_location& loc, const bool ignore_xy);

	const vconfig& cfg_; //config contains WML for a Standard Location Filter
	const gamemap& map_;
	const gamestatus& status_;
	const unit_map& units_;
	const std::vector<team>& teams_;

	struct terrain_filter_cache {
		terrain_filter_cache() :
			parsed_terrain(NULL),
			adjacent_matches(NULL),
			adjacent_match_cache()
		{
		}

		~terrain_filter_cache() {
			delete parsed_terrain;
			delete adjacent_matches;
		}

		//parsed_terrain: optimizes handling of terrain="..."
		t_translation::t_match *parsed_terrain;

		//adjacent_matches: optimize handling of [filter_adjacent_location] for get_locations()
		std::vector< std::set<map_location> > *adjacent_matches;

		//adjacent_match_cache: optimize handling of [filter_adjacent_location] for match()
		std::vector< std::pair<terrain_filter, std::map<map_location,bool> > > adjacent_match_cache;
	};

	terrain_filter_cache cache_;
	size_t max_loop_;
	bool flat_;
};

#endif

