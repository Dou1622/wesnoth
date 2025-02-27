/*
	Copyright (C) 2003 - 2022
	by David White <dave@whitevine.net>
	Part of the Battle for Wesnoth Project https://www.wesnoth.org/

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY.

	See the COPYING file for more details.
*/

/**
 * @file
 *
 * map_display and display: classes which take care of
 * displaying the map and game-data on the screen.
 *
 * The display is divided into two main sections:
 * - the game area, which displays the tiles of the game board, and units on them,
 * - and the side bar, which appears on the right hand side.
 * The side bar display is divided into three sections:
 * - the minimap, which is displayed at the top right
 * - the game status, which includes the day/night image,
 *   the turn number, information about the current side,
 *   and information about the hex currently moused over (highlighted)
 * - the unit status, which displays an image and stats
 *   for the current unit.
 */

#pragma once

class config;
class fake_unit_manager;
class terrain_builder;
class map_labels;
class arrow;
class reports;
class team;
struct overlay;

namespace halo {
	class manager;
}

namespace wb {
	class manager;
}

#include "animated.hpp"
#include "display_context.hpp"
#include "filesystem.hpp"
#include "font/standard_colors.hpp"
#include "game_config.hpp"
#include "gui/core/top_level_drawable.hpp"
#include "halo.hpp"
#include "picture.hpp" //only needed for enums (!)
#include "key.hpp"
#include "time_of_day.hpp"
#include "sdl/rect.hpp"
#include "sdl/surface.hpp"
#include "sdl/texture.hpp"
#include "theme.hpp"
#include "widgets/button.hpp"

#include <boost/circular_buffer.hpp>

#include <functional>
#include <chrono>
#include <cstdint>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <vector>

class gamemap;

class display : public gui2::top_level_drawable
{
public:
	display(const display_context* dc,
		std::weak_ptr<wb::manager> wb,
		reports& reports_object,
		const std::string& theme_id,
		const config& level);

	virtual ~display();
	/**
	 * Returns the display object if a display object exists. Otherwise it returns nullptr.
	 * the display object represents the game gui which handles themewml and drawing the map.
	 * A display object only exists during a game or while the mapeditor is running.
	 */
	static display* get_singleton() { return singleton_ ;}

	bool show_everything() const { return !dont_show_all_ && !is_blindfolded(); }

	const gamemap& get_map() const { return dc_->map(); }

	const std::vector<team>& get_teams() const {return dc_->teams();}

	/** The playing team is the team whose turn it is. */
	std::size_t playing_team() const { return activeTeam_; }

	bool team_valid() const;

	/** The viewing team is the team currently viewing the game. */
	std::size_t viewing_team() const { return currentTeam_; }
	int viewing_side() const { return currentTeam_ + 1; }

	/**
	 * Sets the team controlled by the player using the computer.
	 * Data from this team will be displayed in the game status.
	 */
	void set_team(std::size_t team, bool observe=false);

	/**
	 * set_playing_team sets the team whose turn it currently is
	 */
	void set_playing_team(std::size_t team);


	/**
	 * Cancels all the exclusive draw requests.
	 */
	void clear_exclusive_draws() { exclusive_unit_draw_requests_.clear(); }
	const unit_map& get_units() const {return dc_->units();}

	/**
	 * Allows a unit to request to be the only one drawn in its hex. Useful for situations where
	 * multiple units (one real, multiple temporary) can end up stacked, such as with the whiteboard.
	 * @param loc The location of the unit requesting exclusivity.
	 * @param unit The unit requesting exclusivity.
	 * @return false if there's already an exclusive draw request for this location.
	 */
	bool add_exclusive_draw(const map_location& loc, unit& unit);
	/**
	 * Cancels an exclusive draw request.
	 * @return The id of the unit whose exclusive draw request was canceled, or else
	 *         the empty string if there was no exclusive draw request for this location.
	 */
	std::string remove_exclusive_draw(const map_location& loc);

	/**
	 * Check the overlay_map for proper team-specific overlays to be
	 * displayed/hidden
	 */
	void parse_team_overlays();

	/**
	 * Functions to add and remove overlays from locations.
	 *
	 * An overlay is an image that is displayed on top of the tile.
	 * One tile may have multiple overlays.
	 */
	void add_overlay(const map_location& loc, const std::string& image,
		const std::string& halo="", const std::string& team_name="",const std::string& item_id="",
		bool visible_under_fog = true, float submerge = 0.0f, float z_order = 0);

	/** remove_overlay will remove all overlays on a tile. */
	void remove_overlay(const map_location& loc);

	/** remove_single_overlay will remove a single overlay from a tile */
	void remove_single_overlay(const map_location& loc, const std::string& toDelete);

	/**
	 * Updates internals that cache map size. This should be called when the map
	 * size has changed.
	 */
	void reload_map();

	void change_display_context(const display_context* dc);

	const display_context& get_disp_context() const
	{
		return *dc_;
	}

	halo::manager& get_halo_manager() { return halo_man_; }

	/**
	 * Applies r,g,b coloring to the map.
	 *
	 * The color is usually taken from @ref get_time_of_day unless @a tod_override is given, in which
	 * case that color is used.
	 *
	 * @param tod_override             The ToD to apply to the map instead of that of the current ToD's.
	 */
	void update_tod(const time_of_day* tod_override = nullptr);

	/**
	 * Add r,g,b to the colors for all images displayed on the map.
	 *
	 * Used for special effects like flashes.
	 */
	void adjust_color_overlay(int r, int g, int b);
	tod_color get_color_overlay() const { return color_adjust_; }

	virtual bool in_game() const { return false; }
	virtual bool in_editor() const { return false; }

	/** Virtual functions shadowed in game_display. These are needed to generate reports easily, without dynamic casting. Hope to factor out eventually. */
	virtual const map_location & displayed_unit_hex() const { return map_location::null_location(); }
	virtual int playing_side() const { return -100; } //In this case give an obviously wrong answer to fail fast, since this could actually cause a big bug. */
	virtual const std::set<std::string>& observers() const { static const std::set<std::string> fake_obs = std::set<std::string> (); return fake_obs; }

	/**
	 * mapx is the width of the portion of the display which shows the game area.
	 * Between mapx and x is the sidebar region.
	 */

	const rect& minimap_area() const;
	const rect& palette_area() const;
	const rect& unit_image_area() const;

	/**
	 * Returns the maximum area used for the map
	 * regardless to resolution and view size
	 */
	rect max_map_area() const;

	/**
	 * Returns the area used for the map
	 */
	rect map_area() const;

	/**
	 * Returns the available area for a map, this may differ
	 * from the above. This area will get the background area
	 * applied to it.
	 */
	rect map_outside_area() const;

	/** Check if the bbox of the hex at x,y has pixels outside the area rectangle. */
	static bool outside_area(const SDL_Rect& area, const int x,const int y);

	/**
	 * Function which returns the width of a hex in pixels,
	 * up to where the next hex starts.
	 * (i.e. not entirely from tip to tip -- use hex_size()
	 * to get the distance from tip to tip)
	 */
	static int hex_width() { return (zoom_*3)/4; }

	/**
	 * Function which returns the size of a hex in pixels
	 * (from top tip to bottom tip or left edge to right edge).
	 */
	static int hex_size(){ return zoom_; }

	/** Returns the current zoom factor. */
	static double get_zoom_factor()
	{
		return static_cast<double>(zoom_) / static_cast<double>(game_config::tile_size);
	}

	/** Scale the width and height of a rect by the current zoom factor */
	static SDL_Rect scaled_to_zoom(const SDL_Rect& r)
	{
		const double zf = get_zoom_factor();
		return {r.x, r.y, int(r.w*zf), int(r.h*zf)};
	}

	/**
	 * given x,y co-ordinates of an onscreen pixel, will return the
	 * location of the hex that this pixel corresponds to.
	 * Returns an invalid location if the mouse isn't over any valid location.
	 */
	const map_location hex_clicked_on(int x, int y) const;

	/**
	 * given x,y co-ordinates of a pixel on the map, will return the
	 * location of the hex that this pixel corresponds to.
	 * Returns an invalid location if the mouse isn't over any valid location.
	 */
	const map_location pixel_position_to_hex(int x, int y) const;

	/**
	 * given x,y co-ordinates of the mouse, will return the location of the
	 * hex in the minimap that the mouse is currently over, or an invalid
	 * location if the mouse isn't over the minimap.
	 */
	map_location minimap_location_on(int x, int y);

	const map_location& selected_hex() const { return selectedHex_; }
	const map_location& mouseover_hex() const { return mouseoverHex_; }

	virtual void select_hex(map_location hex);
	virtual void highlight_hex(map_location hex);

	/** Function to invalidate the game status displayed on the sidebar. */
	void invalidate_game_status() { invalidateGameStatus_ = true; }

	/** Functions to get the on-screen positions of hexes. */
	int get_location_x(const map_location& loc) const;
	int get_location_y(const map_location& loc) const;

	/**
	 * Rectangular area of hexes, allowing to decide how the top and bottom
	 * edges handles the vertical shift for each parity of the x coordinate
	 */
	struct rect_of_hexes{
		int left;
		int right;
		int top[2]; // for even and odd values of x, respectively
		int bottom[2];

		/**  very simple iterator to walk into the rect_of_hexes */
		struct iterator {
			iterator(const map_location &loc, const rect_of_hexes &rect)
				: loc_(loc), rect_(rect){}

			/** increment y first, then when reaching bottom, increment x */
			iterator& operator++();
			bool operator==(const iterator &that) const { return that.loc_ == loc_; }
			bool operator!=(const iterator &that) const { return that.loc_ != loc_; }
			const map_location& operator*() const {return loc_;}

			typedef std::forward_iterator_tag iterator_category;
			typedef map_location value_type;
			typedef int difference_type;
			typedef const map_location *pointer;
			typedef const map_location &reference;

			private:
				map_location loc_;
				const rect_of_hexes &rect_;
		};
		typedef iterator const_iterator;

		iterator begin() const;
		iterator end() const;
	};

	/** Return the rectangular area of hexes overlapped by r (r is in screen coordinates) */
	const rect_of_hexes hexes_under_rect(const SDL_Rect& r) const;

	/** Returns the rectangular area of visible hexes */
	const rect_of_hexes get_visible_hexes() const {return hexes_under_rect(map_area());}

	/** Returns true if location (x,y) is covered in shroud. */
	bool shrouded(const map_location& loc) const;

	/** Returns true if location (x,y) is covered in fog. */
	bool fogged(const map_location& loc) const;

	/** Getter for the x,y debug overlay on tiles */
	bool get_draw_coordinates() const { return draw_coordinates_; }
	/** Setter for the x,y debug overlay on tiles */
	void set_draw_coordinates(bool value) { draw_coordinates_ = value; }

	/** Getter for the terrain code debug overlay on tiles */
	bool get_draw_terrain_codes() const { return draw_terrain_codes_; }
	/** Setter for the terrain code debug overlay on tiles */
	void set_draw_terrain_codes(bool value) { draw_terrain_codes_ = value; }

	/** Getter for the number of bitmaps debug overlay on tiles */
	bool get_draw_num_of_bitmaps() const { return draw_num_of_bitmaps_; }
	/** Setter for the terrain code debug overlay on tiles */
	void set_draw_num_of_bitmaps(bool value) { draw_num_of_bitmaps_ = value; }

	/** Capture a (map-)screenshot into a surface. */
	surface screenshot(bool map_screenshot = false);

	/** Marks everything for rendering including all tiles and sidebar.
	  * Also calls redraw observers. */
	void queue_rerender();

	/** Queues repainting to the screen, but doesn't rerender. */
	void queue_repaint();

	/** Adds a redraw observer, a function object to be called when a
	  * full rerender is queued. */
	void add_redraw_observer(std::function<void(display&)> f);

	/** Clear the redraw observers */
	void clear_redraw_observers();

	theme& get_theme() { return theme_; }
	void set_theme(const std::string& new_theme);

	/**
	 * Retrieves a pointer to a theme UI button.
	 *
	 * @note The returned pointer may either be nullptr, meaning the button
	 *       isn't defined by the current theme, or point to a valid
	 *       gui::button object. However, the objects retrieved will be
	 *       destroyed and recreated by draw() method calls. Do *NOT* store
	 *       these pointers for longer than strictly necessary to
	 *       accomplish a specific task before the next screen refresh.
	 */
	std::shared_ptr<gui::button> find_action_button(const std::string& id);
	std::shared_ptr<gui::button> find_menu_button(const std::string& id);

	void create_buttons();

	void layout_buttons();

	void draw_buttons();

	/** Update the given report. Actual drawing is done in draw_report(). */
	void refresh_report(const std::string& report_name, const config * new_cfg=nullptr);

	/**
	 * Draw the specified report.
	 *
	 * If test_run is true, it will simulate the draw without actually
	 * drawing anything. This will add any overflowing information to the
	 * report tooltip, and also registers the tooltip.
	 */
	void draw_report(const std::string& report_name, bool test_run = false);

	/** Draw all reports in the given region.
	  * Returns true if something was drawn, false otherwise. */
	bool draw_reports(const rect& region);

	void draw_minimap_units();

	/** Function to invalidate all tiles. */
	void invalidate_all();

	/** Function to invalidate a specific tile for redrawing. */
	bool invalidate(const map_location& loc);

	bool invalidate(const std::set<map_location>& locs);

	/**
	 * If this set is partially invalidated, invalidate all its hexes.
	 * Returns if any new invalidation was needed
	 */
	bool propagate_invalidation(const std::set<map_location>& locs);

	/** invalidate all hexes under the rectangle rect (in screen coordinates) */
	bool invalidate_locations_in_rect(const SDL_Rect& rect);
	bool invalidate_visible_locations_in_rect(const SDL_Rect& rect);

	/**
	 * Function to invalidate animated terrains and units which may have changed.
	 */
	void invalidate_animations();

	/**
	 * Per-location invalidation called by invalidate_animations()
	 * Extra game per-location invalidation (village ownership)
	 */
	void invalidate_animations_location(const map_location& loc);

	void reset_standing_animations();

	/**
	 * mouseover_hex_overlay_ requires a prerendered texture
	 * and is drawn underneath the mouse's location
	 */
	void set_mouseover_hex_overlay(const texture& image)
		{ mouseover_hex_overlay_ = image; }

	void clear_mouseover_hex_overlay()
		{ mouseover_hex_overlay_.reset(); }

	/** Toggle to continuously redraw the screen. */
	static void toggle_benchmark();

	/**
	 * Toggle to debug foreground terrain.
	 * Separate background and foreground layer
	 * to better spot any error there.
	 */
	static void toggle_debug_foreground();

	terrain_builder& get_builder() {return *builder_;}

	void update_fps_label();
	void clear_fps_label();
	void update_fps_count();

	/** Rebuild all dynamic terrain. */
	void rebuild_all();

	const theme::action* action_pressed();
	const theme::menu*   menu_pressed();

	void set_diagnostic(const std::string& msg);

	double turbo_speed() const;

	void bounds_check_position();
	void bounds_check_position(int& xpos, int& ypos) const;

	/**
	 * Scrolls the display by xmov,ymov pixels.
	 * Invalidation and redrawing will be scheduled.
	 * @return true if the map actually moved.
	 */
	bool scroll(int xmov, int ymov, bool force = false);

	/** Zooms the display in (true) or out (false). */
	bool set_zoom(bool increase);

	/** Sets the display zoom to the specified amount. */
	bool set_zoom(unsigned int amount, const bool validate_value_and_set_index = true);

	static bool zoom_at_max();
	static bool zoom_at_min();

	/** Sets the zoom amount to the default. */
	void toggle_default_zoom();

	bool view_locked() const { return view_locked_; }

	/** Sets whether the map view is locked (e.g. so the user can't scroll away) */
	void set_view_locked(bool value) { view_locked_ = value; }

	enum SCROLL_TYPE { SCROLL, WARP, ONSCREEN, ONSCREEN_WARP };

	/**
	 * Scroll such that location loc is on-screen.
	 * WARP jumps to loc; SCROLL uses scroll speed;
	 * ONSCREEN only scrolls if x,y is offscreen
	 * force : scroll even if preferences tell us not to,
	 * or the view is locked.
	 */
	void scroll_to_tile(const map_location& loc, SCROLL_TYPE scroll_type=ONSCREEN, bool check_fogged=true,bool force = true);

	/**
	 * Scroll such that location loc1 is on-screen.
	 * It will also try to make it such that loc2 is on-screen,
	 * but this is not guaranteed. For ONSCREEN scrolls add_spacing
	 * sets the desired minimum distance from the border in hexes.
	 */
	void scroll_to_tiles(map_location loc1, map_location loc2,
	                     SCROLL_TYPE scroll_type=ONSCREEN, bool check_fogged=true,
	                     double add_spacing=0.0, bool force=true);

	/** Scroll to fit as many locations on-screen as possible, starting with the first. */
	void scroll_to_tiles(const std::vector<map_location>::const_iterator & begin,
	                     const std::vector<map_location>::const_iterator & end,
	                     SCROLL_TYPE scroll_type=ONSCREEN, bool check_fogged=true,
	                     bool only_if_possible=false, double add_spacing=0.0,
	                     bool force=true);
	/** Scroll to fit as many locations on-screen as possible, starting with the first. */
	void scroll_to_tiles(const std::vector<map_location>& locs,
	                     SCROLL_TYPE scroll_type=ONSCREEN, bool check_fogged=true,
	                     bool only_if_possible=false,
	                     double add_spacing=0.0, bool force=true)
	{
		scroll_to_tiles(locs.begin(), locs.end(), scroll_type, check_fogged,
		                only_if_possible, add_spacing, force);
	}

	/** Expose the event, so observers can be notified about map scrolling. */
	events::generic_event &scroll_event() const { return scroll_event_; }

	/** Check if a tile is fully visible on screen. */
	bool tile_fully_on_screen(const map_location& loc) const;

	/** Checks if location @a loc or one of the adjacent tiles is visible on screen. */
	bool tile_nearly_on_screen(const map_location &loc) const;

	/** Prevent the game display from drawing.
	  * Used while story screen is showing to prevent flicker. */
	void set_prevent_draw(bool pd) { prevent_draw_ = pd; }
	bool get_prevent_draw() { return prevent_draw_; }

private:
	bool prevent_draw_ = false;

public:
	/** ToD mask smooth fade */
	void fade_tod_mask(const std::string& old, const std::string& new_);

	/** Screen fade */
	void fade_to(const color_t& color, int duration);
	void set_fade(const color_t& color);

private:
	color_t fade_color_ = {0,0,0,0};

public:
	/*-------------------------------------------------------*/
	/* top_level_drawable interface (called by draw_manager) */
	/*-------------------------------------------------------*/

	/** Update animations and internal state */
	virtual void update() override;

	/** Finalize screen layout. */
	virtual void layout() override;

	/** Update offscreen render buffers. */
	virtual void render() override;

	/** Paint the indicated region to the screen. */
	virtual bool expose(const rect& region) override;

	/** Return the current draw location of the display, on the screen. */
	virtual rect screen_location() override;

private:
	/** Render textures, for intermediate rendering. */
	texture front_ = {};
	texture back_ = {};

	/** Ensure render textures are valid and correct. */
	void update_render_textures();

	/** Draw/redraw the off-map background area.
	  * This updates both render textures. */
	void render_map_outside_area();

	/** Perform rendering of invalidated items. */
	void draw();

public:
	map_labels& labels();
	const map_labels& labels() const;

	/** Holds options for calls to function 'announce' (@ref announce). */
	struct announce_options
	{
		/** Lifetime measured in milliseconds. */
		int lifetime;

		/**
		 * An announcement according these options should replace the
		 * previous announce (typical of fast announcing) or not
		 * (typical of movement feedback).
		 */
		bool discard_previous;

		announce_options()
			: lifetime(1600)
			, discard_previous(false)
		{
		}
	};

	/** Announce a message prominently. */
	void announce(const std::string& msg,
	              const color_t& color = font::GOOD_COLOR,
	              const announce_options& options = announce_options());

	/**
	 * Schedule the minimap for recalculation.
	 * Useful if any terrain in the map has changed.
	 */
	void recalculate_minimap();

	/**
	 * Schedule the minimap to be redrawn.
	 * Useful if units have moved about on the map.
	 */
	void redraw_minimap();

private:
	/** Actually draw the minimap. */
	void draw_minimap();

public:

	virtual const time_of_day& get_time_of_day(const map_location& loc = map_location::null_location()) const;

	virtual bool has_time_area() const {return false;}

	void blindfold(bool flag);
	bool is_blindfolded() const;

	void write(config& cfg) const;

private:
	void read(const config& cfg);

public:
	/** Init the flag list and the team colors used by ~TC */
	void init_flags();

	/** Rebuild the flag list (not team colors) for a single side. */
	void reinit_flags_for_team(const team&);
	void reset_reports(reports& reports_object)
	{
		reports_object_ = &reports_object;
	}

private:
	void init_flags_for_side_internal(std::size_t side, const std::string& side_color);

	int blindfold_ctr_;

protected:
	//TODO sort
	const display_context * dc_;
	halo::manager halo_man_;
	std::weak_ptr<wb::manager> wb_;

	typedef std::map<map_location, std::string> exclusive_unit_draw_requests_t;
	/** map of hexes where only one unit should be drawn, the one identified by the associated id string */
	exclusive_unit_draw_requests_t exclusive_unit_draw_requests_;

	map_location get_middle_location() const;

	/**
	 * Get the clipping rectangle for drawing.
	 * Virtual since the editor might use a slightly different approach.
	 */
	virtual rect get_clip_rect() const;

	/**
	 * Only called when there's actual redrawing to do. Loops through
	 * invalidated locations and redraws them. Derived classes can override
	 * this, possibly to insert pre- or post-processing around a call to the
	 * base class's function.
	 */
	virtual void draw_invalidated();

	/**
	 * Redraws a single gamemap location.
	 */
	virtual void draw_hex(const map_location& loc);

	enum TERRAIN_TYPE { BACKGROUND, FOREGROUND};

	void get_terrain_images(const map_location &loc,
					const std::string& timeid,
					TERRAIN_TYPE terrain_type);

	std::vector<texture> get_fog_shroud_images(const map_location& loc, image::TYPE image_type);

	void scroll_to_xy(int screenxpos, int screenypos, SCROLL_TYPE scroll_type,bool force = true);

	static void fill_images_list(const std::string& prefix, std::vector<std::string>& images);

	static const std::string& get_variant(const std::vector<std::string>& variants, const map_location &loc);

	std::size_t currentTeam_;
	bool dont_show_all_; //const team *viewpoint_;
	/**
	 * Position of the top-left corner of the viewport, in pixels.
	 *
	 * Dependent on zoom_.. For example, ypos_==72 only means we're one
	 * hex below the top of the map when zoom_ == 72 (the default value).
	 */
	int xpos_, ypos_;
	bool view_locked_;
	theme theme_;
	/**
	 * The current zoom, in pixels (on screen) per 72 pixels (in the
	 * graphic assets), i.e., 72 means 100%.
	 */
	static unsigned int zoom_;
	int zoom_index_;
	/** The previous value of zoom_. */
	static unsigned int last_zoom_;
	const std::unique_ptr<fake_unit_manager> fake_unit_man_;
	const std::unique_ptr<terrain_builder> builder_;
	texture minimap_;
	SDL_Rect minimap_location_;
	bool redraw_background_;
	bool invalidateAll_;
	int diagnostic_label_;
	bool invalidateGameStatus_;
	const std::unique_ptr<map_labels> map_labels_;
	reports * reports_object_;

	/** Event raised when the map is being scrolled */
	mutable events::generic_event scroll_event_;

	boost::circular_buffer<unsigned> frametimes_; // in milliseconds
	int current_frame_sample = 0;
	unsigned int fps_counter_;
	std::chrono::seconds fps_start_;
	unsigned int fps_actual_;
	uint32_t last_frame_finished_ = 0u;

	// Not set by the initializer:
	std::map<std::string, rect> reportLocations_;
	std::map<std::string, texture> reportSurfaces_;
	std::map<std::string, config> reports_;
	std::vector<std::shared_ptr<gui::button>> menu_buttons_, action_buttons_;
	std::set<map_location> invalidated_;
	texture mouseover_hex_overlay_;
	// If we're transitioning from one time of day to the next,
	// then we will use these two masks on top of all hexes when we blit.
	texture tod_hex_mask1 = {};
	texture tod_hex_mask2 = {};
	uint8_t tod_hex_alpha1 = 0;
	uint8_t tod_hex_alpha2 = 0;
	std::vector<std::string> fog_images_;
	std::vector<std::string> shroud_images_;

	map_location selectedHex_;
	map_location mouseoverHex_;
	CKey keys_;

	/** Local cache for preferences::animate_map, since it is constantly queried. */
	bool animate_map_;

	/** Local version of preferences::animate_water, used to detect when it's changed. */
	bool animate_water_;

private:

	texture get_flag(const map_location& loc);

	/** Animated flags for each team */
	std::vector<animated<image::locator>> flags_;

	// This vector is a class member to avoid repeated memory allocations in get_terrain_images(),
	// which turned out to be a significant bottleneck while profiling.
	std::vector<texture> terrain_image_vector_;

public:
	/**
	 * The layers to render something on. This value should never be stored
	 * it's the internal drawing order and adding removing and reordering
	 * the layers should be safe.
	 * If needed in WML use the name and map that to the enum value.
	 */
	enum drawing_layer {
		LAYER_TERRAIN_BG,          /**<
		                            * Layer for the terrain drawn behind the
		                            * unit.
		                            */
		LAYER_GRID_TOP,            /**< Top half part of grid image */
		LAYER_MOUSEOVER_OVERLAY,   /**< Mouseover overlay used by editor*/
		LAYER_FOOTSTEPS,           /**< Footsteps showing path from unit to mouse */
		LAYER_MOUSEOVER_TOP,       /**< Top half of image following the mouse */
		LAYER_UNIT_FIRST,          /**< Reserve layers to be selected for WML. */
		LAYER_UNIT_BG = LAYER_UNIT_FIRST+10,             /**< Used for the ellipse behind the unit. */
		LAYER_UNIT_DEFAULT=LAYER_UNIT_FIRST+40,/**<default layer for drawing units */
		LAYER_TERRAIN_FG = LAYER_UNIT_FIRST+50, /**<
		                            * Layer for the terrain drawn in front of
		                            * the unit.
		                            */
		LAYER_GRID_BOTTOM,         /**<
		                            * Used for the bottom half part of grid image.
		                            * Should be under moving units, to avoid masking south move.
		                            */
		LAYER_UNIT_MOVE_DEFAULT=LAYER_UNIT_FIRST+60/**<default layer for drawing moving units */,
		LAYER_UNIT_FG =  LAYER_UNIT_FIRST+80, /**<
		                            * Used for the ellipse in front of the
		                            * unit.
		                            */
		LAYER_UNIT_MISSILE_DEFAULT = LAYER_UNIT_FIRST+90, /**< default layer for missile frames*/
		LAYER_UNIT_LAST=LAYER_UNIT_FIRST+100,
		LAYER_REACHMAP,            /**< "black stripes" on unreachable hexes. */
		LAYER_MOUSEOVER_BOTTOM,    /**< Bottom half of image following the mouse */
		LAYER_FOG_SHROUD,          /**< Fog and shroud. */
		LAYER_ARROWS,              /**< Arrows from the arrows framework. Used for planned moves display. */
		LAYER_ACTIONS_NUMBERING,   /**< Move numbering for the whiteboard. */
		LAYER_SELECTED_HEX,        /**< Image on the selected unit */
		LAYER_ATTACK_INDICATOR,    /**< Layer which holds the attack indicator. */
		LAYER_UNIT_BAR,            /**<
		                            * Unit bars and overlays are drawn on this
		                            * layer (for testing here).
		                            */
		LAYER_MOVE_INFO,           /**< Movement info (defense%, etc...). */
		LAYER_LINGER_OVERLAY,      /**< The overlay used for the linger mode. */
		LAYER_BORDER,              /**< The border of the map. */
	};

	/**
	 * Draw an image at a certain location.
	 * x,y: pixel location on screen to draw the image
	 * image: the image to draw
	 * reverse: if the image should be flipped across the x axis
	 * greyscale: used for instance to give the petrified appearance to a unit image
	 * alpha: the merging to use with the background
	 * blendto: blend to this color using blend_ratio
	 * submerged: the amount of the unit out of 1.0 that is submerged
	 *            (presumably under water) and thus shouldn't be drawn
	 */
	void render_image(int x, int y, const display::drawing_layer drawing_layer,
			const map_location& loc, const image::locator& i_locator,
			bool hreverse=false, bool greyscale=false,
			uint8_t alpha=SDL_ALPHA_OPAQUE, double highlight=0.0,
			color_t blendto={0,0,0}, double blend_ratio=0,
			double submerged=0.0, bool vreverse=false);

	/**
	 * Draw text on a hex. (0.5, 0.5) is the center.
	 * The font size is adjusted to the zoom factor.
	 */
	void draw_text_in_hex(const map_location& loc,
		const drawing_layer layer, const std::string& text, std::size_t font_size,
		color_t color, double x_in_hex=0.5, double y_in_hex=0.5);

protected:

	//TODO sort
	std::size_t activeTeam_;

	/**
	 * In order to render a hex properly it needs to be rendered per row. On
	 * this row several layers need to be drawn at the same time. Mainly the
	 * unit and the background terrain. This is needed since both can spill
	 * in the next hex. The foreground terrain needs to be drawn before to
	 * avoid decapitation a unit.
	 *
	 * In other words:
	 * for every layer
	 *   for every row (starting from the top)
	 *     for every hex in the row
	 *       ...
	 *
	 * this is modified to:
	 * for every layer group
	 *   for every row (starting from the top)
	 *     for every layer in the group
	 *       for every hex in the row
	 *         ...
	 *
	 * * textures are rendered per level in a map.
	 * * Per level the items are rendered per location these locations are
	 *   stored in the drawing order required for units.
	 * * every location has a vector with textures, each with its own screen
	 *   coordinate to render at.
	 * * every vector element has a vector with textures to render.
	 */
	class drawing_buffer_key
	{
	private:
		unsigned int key_;

		// FIXME: temporary method. Group splitting should be made
		// public into the definition of drawing_layer
		//
		// The drawing is done per layer_group, the range per group is [low, high).
		static inline const std::array layer_groups {
			LAYER_TERRAIN_BG,
			LAYER_UNIT_FIRST,
			LAYER_UNIT_MOVE_DEFAULT,
			// Make sure the movement doesn't show above fog and reachmap.
			LAYER_REACHMAP
		};

	public:
		drawing_buffer_key(const map_location &loc, drawing_layer layer);

		bool operator<(const drawing_buffer_key &rhs) const { return key_ < rhs.key_; }
	};

	/** Helper structure for rendering the terrains. */
	class blit_helper
	{
	public:
		// We don't want to copy this.
		// It's expensive when done frequently due to the texture vector.
		blit_helper(const blit_helper&) = delete;

		blit_helper(const drawing_layer layer, const map_location& loc,
				const SDL_Rect& dest, const texture& tex)
			: dest_(dest), tex_(1, tex), key_(loc, layer)
		{}

		blit_helper(const drawing_layer layer, const map_location& loc,
				const SDL_Rect& dest, const std::vector<texture>& tex)
			: dest_(dest), tex_(tex), key_(loc, layer)
		{}

		const SDL_Rect& dest() const { return dest_; }
		const std::vector<texture> &tex() const { return tex_; }

		bool operator<(const blit_helper &rhs) const { return key_ < rhs.key_; }

	public:
		// Auxiliary parameters, can be modified directly as required.

		/** Whether to mirror horizontally on draw */
		bool hflip = false;
		/** Whether to mirror vertically on draw */
		bool vflip = false;
		/** An alpha modifier to apply when drawing. 0-255. */
		uint8_t alpha_mod = SDL_ALPHA_OPAQUE;
		/** Colour modifiers. Multiply colour. 0 = 0.0, 255 = 1.0. */
		uint8_t r_mod = 255;
		uint8_t g_mod = 255;
		uint8_t b_mod = 255;
		/** Strength of highlight effect to apply, if any. */
		uint8_t highlight = 0;

		// Or they can be set in a chain.
		blit_helper& set_color_and_alpha(color_t c)
		{
			this->r_mod = c.r; this->g_mod = c.g; this->b_mod = c.b;
			this->alpha_mod = c.a;
			return *this;
		}
		blit_helper& set_color_and_alpha(
			uint8_t r, uint8_t g, uint8_t b, uint8_t a)
		{
			this->r_mod = r; this->g_mod = g; this->b_mod = b;
			this->alpha_mod = a;
			return *this;
		}
		blit_helper& set_color(color_t c)
		{
			this->r_mod = c.r; this->g_mod = c.g; this->b_mod = c.b;
			return *this;
		}
		blit_helper& set_color(uint8_t r, uint8_t g, uint8_t b)
		{
			this->r_mod = r; this->g_mod = g; this->b_mod = b;
			return *this;
		}
		blit_helper& set_alpha(uint8_t alpha)
		{
			this->alpha_mod = alpha; return *this;
		}
		blit_helper& set_hflip(bool hflip)
		{
			this->hflip = hflip; return *this;
		}
		blit_helper& set_vflip(bool vflip)
		{
			this->vflip = vflip; return *this;
		}
		blit_helper& set_highlight(uint8_t highlight)
		{
			this->highlight = highlight; return *this;
		}

	private:
		// Core info is set on creation.

		/** The location on screen to draw to, in drawing coordinates. */
		SDL_Rect dest_;
		/** One or more textures to render. */
		std::vector<texture> tex_;
		// TODO: could also add blend mode and rotation if desirable
		/** Allows ordering of draw calls by layer and location. */
		drawing_buffer_key key_;
	};

	typedef std::list<blit_helper> drawing_buffer;
	drawing_buffer drawing_buffer_;

public:
	/**
	 * Add an item to the drawing buffer.
	 *
	 * This returns a blit_helper reference with several extra fields that can
	 * be modified as necessary. In particular hflip, vflip and alpha_mod
	 * have been moved to this helper. Fields that can be modified are
	 * available as public members of blit_helper.
	 *
	 * @param layer              The layer to draw on.
	 * @param loc                The hex the image belongs to, needed for the
	 *                           drawing order.
	 * @param dest               The target destination on screen,
	 *                           in drawing coordinates.
	 * @param tex                The texture to use.
	 */
	blit_helper& drawing_buffer_add(const drawing_layer layer,
			const map_location& loc, const SDL_Rect& dest, const texture& tex);

	blit_helper& drawing_buffer_add(const drawing_layer layer,
			const map_location& loc, const SDL_Rect& dest,
			const std::vector<texture> &tex);

protected:

	/** Draws the drawing_buffer_ and clears it. */
	void drawing_buffer_commit();

	/** Clears the drawing buffer. */
	void drawing_buffer_clear();

	/** Redraws all panels intersecting the given region.
	  * Returns true if something was drawn, false otherwise. */
	bool draw_all_panels(const rect& region);

private:
	void draw_panel(const theme::panel& panel);
	void draw_label(const theme::label& label);

protected:

	/**
	 * Initiate a redraw.
	 *
	 * Invalidate controls and panels when changed after they have been drawn
	 * initially. Useful for dynamic theme modification.
	 */
	void draw_init();
	void draw_wrap(bool update,bool force);

	/** Used to indicate to drawing functions that we are doing a map screenshot */
	bool map_screenshot_;

public: //operations for the arrow framework

	void add_arrow(arrow&);

	void remove_arrow(arrow&);

	/** Called by arrow objects when they change. You should not need to call this directly. */
	void update_arrow(arrow & a);

protected:

	// Tiles lit for showing where unit(s) can reach
	typedef std::map<map_location,unsigned int> reach_map;
	reach_map reach_map_;
	reach_map reach_map_old_;
	bool reach_map_changed_;
	void process_reachmap_changes();

	typedef std::map<map_location, std::vector<overlay>> overlay_map;

	virtual overlay_map& get_overlays() = 0;

private:
	/** Handle for the label which displays frames per second. */
	int fps_handle_;
	/** Count work done for the debug info displayed under fps */
	int invalidated_hexes_;
	int drawn_hexes_;

	std::vector<std::function<void(display&)>> redraw_observers_;

	/** Debug flag - overlay x,y coords on tiles */
	bool draw_coordinates_;
	/** Debug flag - overlay terrain codes on tiles */
	bool draw_terrain_codes_;
	/** Debug flag - overlay number of bitmaps on tiles */
	bool draw_num_of_bitmaps_;

	typedef std::list<arrow*> arrows_list_t;
	typedef std::map<map_location, arrows_list_t > arrows_map_t;
	/** Maps the list of arrows for each location */
	arrows_map_t arrows_map_;

	tod_color color_adjust_;

	std::vector<std::tuple<int, int, int>> fps_history_;

protected:
	static display * singleton_;
};

struct blindfold
{
	blindfold(display& d, bool lock=true) : display_(d), blind(lock) {
		if(blind) {
			display_.blindfold(true);
		}
	}

	~blindfold() {
		unblind();
	}

	void unblind() {
		if(blind) {
			display_.blindfold(false);
			display_.queue_rerender();
			blind = false;
		}
	}

private:
	display& display_;
	bool blind;
};
