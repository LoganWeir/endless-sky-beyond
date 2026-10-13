/* Arena.h
Copyright (c) 2026 by Logan Weir

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <list>
#include <map>
#include <memory>

class Outfit;
class PlayerInfo;
class Ship;



// Repeating one-on-one duels for testing combat. While the player has the
// "arena: active" condition, the flagship and the first hostile ship in its
// system face each other "arena: distance" apart (1500 by default). As soon as
// either one is disabled or destroyed, the round ends: both ships are restored
// to full health and ammunition and placed back at their starting points.
class Arena {
public:
	// Check the current round and start a new one if it has ended. Returns true
	// if the ships were reset this step, in which case the caller should clear
	// projectiles in flight and drop the events for the ships' defeat.
	bool Step(PlayerInfo &player, const std::list<std::shared_ptr<Ship>> &ships);
	// Check if this ship is one of the two duelists.
	bool IsDuelist(const Ship *ship) const;


private:
	void StartRound(Ship &flagship, Ship &opponent, double distance);


private:
	std::weak_ptr<Ship> flagship;
	std::weak_ptr<Ship> opponent;
	// Each ship's outfits when the duel began, so spent ammunition can be restored.
	std::map<const Outfit *, int> flagshipOutfits;
	std::map<const Outfit *, int> opponentOutfits;

	int round = 0;
	int roundSteps = 0;
	int wins = 0;
	int losses = 0;
	int draws = 0;
};
