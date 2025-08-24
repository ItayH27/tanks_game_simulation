# Algorithm Implementation

The algorithm implementation consist two elements: `TankAlgorithm` and `Player.`

### Tank Algorithm

Each tank of the algorithm is driven by a decision system that controls movement, shooting, and reactions to battlefield events.

In our implementation, we designed a tank that actively pursues the closest enemy using a BFS search. Its behavior follows these rules:
- Recomputes the path to the nearest enemy every five turns.
- Fires when the opponent is directly in its line of sight.
- Evades incoming shells.
- Shares internal state through the `BattleInfo` object.

### Player

The Player manages its tanks and processes satellite battlefield data.

On `GetBattleInfo`, the `SatelliteView` is passed to the `Player`, which updates each tank’s `BattleInfo`.
No pointers or references to the `SatelliteView` or `BattleInfo` are retained beyond their allowed lifetime.

In our implementation, we created an **Observant player**: 
- The player always reveals all the enemy tanks to his tanks, allowing them to choose thier target on their own.
- The player passes the `Battleinfo` as is.