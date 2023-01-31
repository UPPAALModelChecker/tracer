#ifndef TRACER_TRACER_HPP
#define TRACER_TRACER_HPP

/** tracer - Utility for printing UPPAAL XTR trace files.
   Copyright (C) 2006 Uppsala University and Aalborg University.
   Copyright (C) 2017 Aalborg University.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA
*/

#include <limits>
#include <map>
#include <string>
#include <variant>
#include <vector>

/** Represents a memory cell. */
struct cell_t
{
    /** Name of cell. Not all types have names. */
    std::string name;

    struct const_t
    {
        int value;
    };
    struct clock_t
    {
        int nr;
    };
    struct integer_t
    {
        int min, max, init, nr;
    };
    struct meta_t
    {
        int min, max, init, nr;
    };
    struct sys_meta_t
    {
        int min, max;
    };
    enum flags_t : int { NONE, COMMITTED, URGENT };
    struct location_t
    {
        flags_t flags;
        int process{-1}, invariant{-1};
    };
    struct fixed_t
    {
        int min, max;
    };
    struct cost_t
    {};
    std::variant<const_t, clock_t, integer_t, meta_t, sys_meta_t, location_t, fixed_t, cost_t> data;
};

/** Represents a process. */
struct process_t
{
    int initial{-1};             ///< initial location index in locations
    std::string name;            ///< process name
    std::vector<int> locations;  ///< location index in model_t::layout
    std::vector<int> edges;      ///< edge index in model_t::layout
};

/** Represents an edge. */
struct edge_t
{
    int process{-1};  ///< process index in model_t::processes
    int source{-1};   ///< source location index in process_t::location
    int target{-1};   ///< target location index in process_t::location
    int guard{-1};    ///< guard expression index in model_t::layout
    int sync{-1};     ///< synchronization expression index in model_t::layout
    int update{-1};   ///< update expression index in model_t::layout
};

/** The UPPAAL model as in the intermediate format. */
struct model_t
{
    std::vector<cell_t> layout;  ///< declarations (locations, edges, labels etc)
    std::vector<int> instructions;
    std::vector<process_t> processes;
    std::vector<edge_t> edges;
    std::map<int, std::string> expressions;

    std::vector<std::string> integers;  ///< integer variable names
    std::vector<std::string> clocks;    ///< clock variable names
    std::istream& read(std::istream&);  ///< parses the model from input stream
    /** clears all members */
    void clear()
    {
        layout.clear();
        instructions.clear();
        processes.clear();
        edges.clear();
        expressions.clear();
    }
};

/** A bound for a clock constraint. A bound consists of a value and a
 * bit indicating whether the bound is strict. */
struct bound_t
{
    int value : 31;   ///< The value of the bound
    bool strict : 1;  ///< True if the bound is strict
};

/** The bound (infinity, <). */
static constexpr bound_t infinity = {std::numeric_limits<int32_t>::max() >> 1, true};

/** The bound (0, <=). */
static constexpr bound_t zero = {0, false};

/** A symbolic state: process location vector, integer values and a DBM */
struct State
{
    std::vector<int> locations;  ///< location index into model_t::processes
    std::vector<int> integers;   ///< values for integers in model_t::integers
    std::vector<bound_t> dbm;    ///< bounds over clocks in model_t::clocks

    /// Sets the bound for (#i - #j) clock difference
    void set_bound(size_t clock_count, int i, int j, bound_t bound);
    /// Gets the bound over (#i - #j) clock difference
    const bound_t& get_bound(size_t clock_count, int i, int j) const;
    std::ostream& print(const model_t&, std::ostream&) const;
    std::istream& read(const model_t&, std::istream&);
};

/** A transition edge (syntactic edge with values) */
struct Edge
{
    int process{-1};            ///< process index in model_t::processes
    int edge{-1};               ///< syntactic edge index in model_t::edges
    std::vector<int> select{};  ///< values for select statement on the edge
};

/** A transition consists of one or more edges. Edges are indexes from
 * 0 in the order they appear in the input file. */
struct Transition
{
    std::vector<Edge> edges{};
    std::ostream& print(const model_t&, std::ostream&) const;
    std::istream& read(const model_t&, std::istream&);
};

struct Successor
{
    Transition transition;
    State state;
    Successor(Transition transition, State state): transition{std::move(transition)}, state{std::move(state)} {}
};

struct trace_t
{
    State initial{};
    std::vector<Successor> steps;
    std::istream& read(const model_t&, std::istream&);
    std::ostream& print(const model_t&, std::ostream&) const;
};

#endif  // TRACER_TRACER_HPP
