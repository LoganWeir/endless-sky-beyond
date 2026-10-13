/* Arena.cpp
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

#include "Arena.h"

#include "Angle.h"
#include "ConditionsStore.h"
#include "text/Format.h"
#include "GameData.h"
#include "Government.h"
#include "Messages.h"
#include "PlayerInfo.h"
#include "Point.h"
#include "Ship.h"

#include <string>

using namespace std;

namespace {
	const double DEFAULT_DISTANCE = 1500.;

	// Repair a ship completely, even if it has begun exploding, and give back
	// any outfits (such as ammunition) it has used up since the duel began.
	void Restore(Ship &ship, const map<const Outfit *, int> &outfits)
	{
		if(ship.IsDestroyed())
			ship.Restore();
		else
			ship.Recharge();

		map<const Outfit *, int> current = ship.Outfits();
		for(const auto &[outfit, count] : outfits)
		{
			auto it = current.find(outfit);
			int difference = count - (it == current.end() ? 0 : it->second);
			if(difference)
				ship.AddOutfit(outfit, difference);
		}
	}
}



bool Arena::Step(PlayerInfo &player, const list<shared_ptr<Ship>> &ships)
{
	shared_ptr<Ship> you = player.FlagshipPtr();
	if(!you || !player.Conditions().Get("arena: active"))
	{
		*this = Arena();
		return false;
	}
	// Wait until the flagship has finished taking off.
	if(you->GetSystem() != player.GetSystem() || you->Zoom() < 1.)
		return false;

	double distance = player.Conditions().Get("arena: distance");
	if(distance <= 0.)
		distance = DEFAULT_DISTANCE;

	shared_ptr<Ship> them = opponent.lock();
	if(you != flagship.lock() || !them || them->GetSystem() != you->GetSystem())
	{
		*this = Arena();
		for(const shared_ptr<Ship> &ship : ships)
			if(ship != you && !ship->IsYours() && ship->GetSystem() == you->GetSystem()
					&& ship->GetGovernment() && ship->GetGovernment()->IsEnemy())
			{
				them = ship;
				break;
			}
		if(!them)
			return false;

		flagship = you;
		opponent = them;
		flagshipOutfits = you->Outfits();
		opponentOutfits = them->Outfits();
		StartRound(*you, *them, distance);
		return true;
	}

	++roundSteps;
	bool youLost = you->IsDisabled() || you->IsDestroyed();
	bool theyLost = them->IsDisabled() || them->IsDestroyed();
	if(!youLost && !theyLost)
		return false;

	string result;
	if(youLost && theyLost)
	{
		++draws;
		result = "draw";
	}
	else if(theyLost)
	{
		++wins;
		result = "you won";
	}
	else
	{
		++losses;
		result = "you lost";
	}
	Messages::Add({"Round " + to_string(round) + ": " + result + " in "
		+ Format::Number(roundSteps / 60., 1, false) + " seconds. Score: " + to_string(wins) + " won, "
		+ to_string(losses) + " lost" + (draws ? ", " + to_string(draws) + " drawn." : "."),
		GameData::MessageCategories().Get("normal")});

	Restore(*you, flagshipOutfits);
	Restore(*them, opponentOutfits);
	StartRound(*you, *them, distance);
	return true;
}



bool Arena::IsDuelist(const Ship *ship) const
{
	return ship && (ship == flagship.lock().get() || ship == opponent.lock().get());
}



// Place the two ships on either side of the system's center, facing each other.
void Arena::StartRound(Ship &you, Ship &them, double distance)
{
	you.Place(Point(-.5 * distance, 0.), Point(), Angle(90.), false);
	them.Place(Point(.5 * distance, 0.), Point(), Angle(270.), false);
	++round;
	roundSteps = 0;
}
