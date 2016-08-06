/*
 * This file is part of BBP Pairings, a Swiss-system chess tournament engine
 * Copyright (C) 2016  Jeremy Bierema
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.0, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <algorithm>
#include <cassert>
#include <deque>
#include <functional>
#include <iomanip>
#include <limits>
#include <list>
#include <new>
#include <ostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

#include <tournament/tournament.h>
#include <utility/uintstringconversion.h>

#include "burstein.h"
#include "common.h"

namespace swisssystems
{
  namespace
  {
    /**
     * Advance iterator to the next played game, stopping at endIterator if none
     * is found.
     */
    decltype(std::declval<tournament::Player>().matches)::const_reverse_iterator
      skipUnplayedGames(
        decltype(std::declval<tournament::Player>().matches)
            ::const_reverse_iterator
          iterator,
        decltype(std::declval<tournament::Player>().matches)
            ::const_reverse_iterator
          endIterator)
    {
      while (iterator != endIterator && !iterator->gameWasPlayed)
      {
        ++iterator;
      }
      return iterator;
    }

    /**
     * Construct the deque of headers for the checklist file, given the headers
     * specific to the Swiss system used.
     */
    std::deque<std::string> getHeader(
      const std::deque<std::string> specialtyHeaders,
      const tournament::Tournament &tournament)
    {
      if (tournament.playedRounds >= std::string::npos)
      {
        throw std::length_error("");
      }
      std::deque<std::string> result{
        "ID",
        "Pts",
        std::string(tournament.playedRounds + 1u, '-'),
        "Pref"
      };
      result.insert(
        result.end(),
        specialtyHeaders.begin(),
        specialtyHeaders.end());
      result.push_back("");
      for (
        tournament::round_index roundIndex{ };
        roundIndex < tournament.playedRounds;
        ++roundIndex)
      {
        result.push_back(
          'R' + utility::uintstringconversion::toString(roundIndex + 1u));
      }
      return result;
    }

    /**
     * Get a row of values for the checklist file, given the values specific to
     * the Swiss system used.
     */
    std::deque<std::string> getRow(
      const std::deque<std::string> specialtyColumns,
      const tournament::Player &player,
      const tournament::Tournament &tournament)
    {
      std::string colorString;
      for (const tournament::Match &match : player.matches)
      {
        if (match.gameWasPlayed)
        {
          colorString += match.color == tournament::COLOR_WHITE ? 'W' : 'B';
        }
      }
      const bool preferenceIsWhite =
        player.colorPreference == tournament::COLOR_WHITE;
      std::deque<std::string> result{
        utility::uintstringconversion::toString(player.id + 1u),
        utility::uintstringconversion
          ::toString(player.scoreWithAcceleration(), 1),
        colorString,
        player.absoluteColorPreference ? preferenceIsWhite ? "W " : "B "
          : player.strongColorPreference ? preferenceIsWhite ? "(W)" : "(B)"
          : player.colorPreference == tournament::COLOR_NONE ? "A "
          : preferenceIsWhite ? "w " : "b "
      };
      result.insert(
        result.end(),
        specialtyColumns.begin(),
        specialtyColumns.end());
      result.push_back("");
      for (
        tournament::round_index roundIndex{ };
        roundIndex < tournament.playedRounds;
        ++roundIndex)
      {
        result.push_back(
          player.matches[roundIndex].gameWasPlayed
            ? utility::uintstringconversion
                ::toString(player.matches[roundIndex].opponent + 1u)
            : "");
      }
      return result;
    }

    /**
     * Make all the column widths large enough for the provided data.
     */
    void updateColumnWidths(
      std::deque<unsigned int> &widths,
      const std::deque<std::string> &data)
    {
      auto widthsIterator = widths.begin();
      for (const std::string &string : data)
      {
        assert(widthsIterator != widths.end());
        if (string.length() > std::numeric_limits<int>::max())
        {
          throw std::length_error("");
        }
        *widthsIterator =
          std::max<unsigned int>(*widthsIterator, string.length());
        ++widthsIterator;
      }
      assert(widthsIterator == widths.end());
    }

    /**
     * Output the given header or row, using the specified column widths.
     */
    void printRow(
      std::ostream &stream,
      const std::deque<std::string> &row,
      const std::deque<unsigned int> &widths)
    {
      stream << std::setfill(' ') << std::right;
      auto widthIterator = widths.begin();
      for (const std::string &string : row)
      {
        stream << std::setw(*widthIterator) << string << '\t';
        ++widthIterator;
      }
    }
  }

  /**
   * Sort the pairings according to the rules for ordering pairings when
   * published.
   */
  void sortResults(
    std::list<Pairing> &pairs,
    const tournament::Tournament &tournament)
  {
    pairs.sort(
      [&tournament](const Pairing &pair0, const Pairing &pair1)
      {
        const tournament::player_index &higherInPair0 =
          tournament::unacceleratedScoreRankCompare(
            &tournament.players[pair0.white],
            &tournament.players[pair0.black]
          ) ? pair0.black
            : pair0.white;
        const tournament::player_index &higherInPair1 =
          tournament::unacceleratedScoreRankCompare(
            &tournament.players[pair1.white],
            &tournament.players[pair1.black]
          ) ? pair1.black
            : pair1.white;

        const tournament::player_index lowerInPair0 =
          pair0.white == higherInPair0 ? pair0.black : pair0.white;
        const tournament::player_index lowerInPair1 =
          pair1.white == higherInPair1 ? pair1.black : pair1.white;

        return
          std::make_tuple(
            pair0.white == pair0.black,
            tournament.players[higherInPair1].scoreWithoutAcceleration,
            tournament.players[lowerInPair1].scoreWithoutAcceleration,
            tournament.players[higherInPair0].rankIndex
          ) < std::make_tuple(
                pair1.white == pair1.black,
                tournament.players[higherInPair0].scoreWithoutAcceleration,
                tournament.players[lowerInPair0].scoreWithoutAcceleration,
                tournament.players[higherInPair1].rankIndex);
      }
    );
  }

  /**
   * Find the colors of the two players on the most recent round in which they
   * differed.
   */
  void findFirstColorDifference(
    const tournament::Player &player0,
    const tournament::Player &player1,
    tournament::Color &color0,
    tournament::Color &color1)
  {
    auto iterator0 =
      skipUnplayedGames(player0.matches.rbegin(), player0.matches.rend());
    auto iterator1 =
      skipUnplayedGames(player1.matches.rbegin(), player1.matches.rend());
    while (
      iterator0 != player0.matches.rend()
        && iterator1 != player1.matches.rend()
        && iterator0->color == iterator1->color)
    {
      iterator0 = skipUnplayedGames(++iterator0, player0.matches.rend());
      iterator1 = skipUnplayedGames(++iterator1, player1.matches.rend());
    }
    color0 =
      iterator0 == player0.matches.rend()
        ? tournament::COLOR_NONE
        : iterator0->color;
    color1 =
      iterator1 == player1.matches.rend()
        ? tournament::COLOR_NONE
        : iterator1->color;
  }

  /**
   * Produce the checklist file, given a function that can provide the values
   * for the columns specific to the Swiss system used, as well as the order
   * in which players should appear. Extra line breaks will be added between
   * scoregroups.
   */
  void printChecklist(
    std::ostream &ostream,
    const std::deque<std::string> &specialtyHeaders,
    const std::function<std::deque<std::string>(const tournament::Player &)>
      &specialtyValues,
    const tournament::Tournament &tournament,
    const std::list<const tournament::Player *> &orderedPlayers)
  {
    try
    {
      // Compute the column widths.
      std::deque<unsigned int> columnWidths;
      for (
        const std::string &string
          : getHeader(specialtyHeaders, tournament))
      {
        if (string.length() > std::numeric_limits<int>::max())
        {
          throw std::length_error("");
        }
        columnWidths.push_back(string.length());
      }
      for (const tournament::Player *const player : orderedPlayers)
      {
        updateColumnWidths(
          columnWidths,
          getRow(specialtyValues(*player), *player, tournament)
        );
      }

      // Output the checklist.
      ostream << std::endl;
      printRow(
        ostream,
        getHeader(specialtyHeaders, tournament),
        columnWidths);
      const tournament::Player *previousPlayer{ };
      for (const tournament::Player *const player : orderedPlayers)
      {
        ostream << std::endl;
        if (
          !previousPlayer
            || previousPlayer->scoreWithAcceleration()
                != player->scoreWithAcceleration())
        {
          ostream << std::endl;
        }
        printRow(
          ostream,
          getRow(specialtyValues(*player), *player, tournament),
          columnWidths);
        previousPlayer = player;
      }
    }
    catch (const std::length_error &)
    {
      ostream
        << "Error: The build does not support checklists for tournaments this "
            "large.";
    }
    catch (const std::bad_alloc &)
    {
      ostream
        << "Error: There was not enough memory to construct the checklist.";
    }
    ostream << std::endl << std::endl << std::endl;
  }

  constexpr burstein::BursteinInfo bursteinInfo{ };

  /**
   * Retrieve the Info object for the specified SwissSystem.
   */
  const Info &getInfo(const SwissSystem swissSystem)
  {
    switch (swissSystem)
    {
    case BURSTEIN:
      return bursteinInfo;
      break;
    default:
      assert(false);
    }
    return bursteinInfo;
  }
}
