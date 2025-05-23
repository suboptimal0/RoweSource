#include "global.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_arena.h"
#include "battle_pyramid.h"
#include "battle_util.h"
#include "battle_controllers.h"
#include "battle_interface.h"
#include "battle_setup.h"
#include "party_menu.h"
#include "pokemon.h"
#include "international_string_util.h"
#include "item.h"
#include "util.h"
#include "battle_scripts.h"
#include "random.h"
#include "text.h"
#include "safari_zone.h"
#include "sound.h"
#include "sprite.h"
#include "strings.h"
#include "string_util.h"
#include "task.h"
#include "trig.h"
#include "window.h"
#include "battle_message.h"
#include "battle_ai_script_commands.h"
#include "event_data.h"
#include "link.h"
#include "malloc.h"
#include "berry.h"
#include "pokedex.h"
#include "mail.h"
#include "field_weather.h"
#include "constants/abilities.h"
#include "constants/battle_anim.h"
#include "constants/battle_config.h"
#include "constants/battle_move_effects.h"
#include "constants/battle_script_commands.h"
#include "constants/battle_string_ids.h"
#include "constants/berry.h"
#include "constants/hold_effects.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/songs.h"
#include "constants/species.h"
#include "constants/trainers.h"
#include "constants/weather.h"
#include "level_scaling.h"
#include "printf.h"
#include "mgba.h"

/*
NOTE: The data and functions in this file up until (but not including) sSoundMovesTable
are actually part of battle_main.c. They needed to be moved to this file in order to
match the ROM; this is also why sSoundMovesTable's declaration is in the middle of
functions instead of at the top of the file with the other declarations.
*/

static void TryToRevertIceFace(u8 battlerId);

extern const u8 *const gBattleScriptsForMoveEffects[];
extern const u8 *const gBattlescriptsForBallThrow[];
extern const u8 *const gBattlescriptsForRunningByItem[];
extern const u8 *const gBattlescriptsForUsingItem[];
extern const u8 *const gBattlescriptsForSafariActions[];

static const u8 sPkblToEscapeFactor[][3] = {{0, 0, 0}, {3, 5, 0}, {2, 3, 0}, {1, 2, 0}, {1, 1, 0}};
static const u8 sGoNearCounterToCatchFactor[] = {4, 3, 2, 1};
static const u8 sGoNearCounterToEscapeFactor[] = {4, 4, 4, 4};

void HandleAction_UseMove(void)
{
    u32 i, side, moveType, var = 4;

    gBattlerAttacker = gBattlerByTurnOrder[gCurrentTurnActionNumber];
    if (gBattleStruct->field_91 & gBitTable[gBattlerAttacker] || !IsBattlerAlive(gBattlerAttacker))
    {
        gCurrentActionFuncId = B_ACTION_FINISHED;
        return;
    }

    gIsCriticalHit = FALSE;
    gBattleStruct->atkCancellerTracker = 0;
    gMoveResultFlags = 0;
    gMultiHitCounter = 0;
    gBattleCommunication[6] = 0;
    gBattleScripting.savedMoveEffect = 0;
    gCurrMovePos = gChosenMovePos = *(gBattleStruct->chosenMovePositions + gBattlerAttacker);

    // choose move
    if (gProtectStructs[gBattlerAttacker].noValidMoves)
    {
        gProtectStructs[gBattlerAttacker].noValidMoves = 0;
        gCurrentMove = gChosenMove = MOVE_STRUGGLE;
        gHitMarker |= HITMARKER_NO_PPDEDUCT;
        *(gBattleStruct->moveTarget + gBattlerAttacker) = GetMoveTarget(MOVE_STRUGGLE, 0);
    }
    else if (gBattleMons[gBattlerAttacker].status2 & STATUS2_MULTIPLETURNS || gBattleMons[gBattlerAttacker].status2 & STATUS2_RECHARGE)
    {
        gCurrentMove = gChosenMove = gLockedMoves[gBattlerAttacker];
    }
    // encore forces you to use the same move
    else if (gDisableStructs[gBattlerAttacker].encoredMove != MOVE_NONE
             && gDisableStructs[gBattlerAttacker].encoredMove == gBattleMons[gBattlerAttacker].moves[gDisableStructs[gBattlerAttacker].encoredMovePos])
    {
        gCurrentMove = gChosenMove = gDisableStructs[gBattlerAttacker].encoredMove;
        gCurrMovePos = gChosenMovePos = gDisableStructs[gBattlerAttacker].encoredMovePos;
        *(gBattleStruct->moveTarget + gBattlerAttacker) = GetMoveTarget(gCurrentMove, 0);
    }
    // check if the encored move wasn't overwritten
    else if (gDisableStructs[gBattlerAttacker].encoredMove != MOVE_NONE
             && gDisableStructs[gBattlerAttacker].encoredMove != gBattleMons[gBattlerAttacker].moves[gDisableStructs[gBattlerAttacker].encoredMovePos])
    {
        gCurrMovePos = gChosenMovePos = gDisableStructs[gBattlerAttacker].encoredMovePos;
        gCurrentMove = gChosenMove = gBattleMons[gBattlerAttacker].moves[gCurrMovePos];
        gDisableStructs[gBattlerAttacker].encoredMove = MOVE_NONE;
        gDisableStructs[gBattlerAttacker].encoredMovePos = 0;
        gDisableStructs[gBattlerAttacker].encoreTimer = 0;
        *(gBattleStruct->moveTarget + gBattlerAttacker) = GetMoveTarget(gCurrentMove, 0);
    }
    else if (gBattleMons[gBattlerAttacker].moves[gCurrMovePos] != gChosenMoveByBattler[gBattlerAttacker])
    {
        gCurrentMove = gChosenMove = gBattleMons[gBattlerAttacker].moves[gCurrMovePos];
        *(gBattleStruct->moveTarget + gBattlerAttacker) = GetMoveTarget(gCurrentMove, 0);
    }
    else
    {
        gCurrentMove = gChosenMove = gBattleMons[gBattlerAttacker].moves[gCurrMovePos];
    }

    if (gBattleMons[gBattlerAttacker].hp != 0)
    {
        if (GetBattlerSide(gBattlerAttacker) == B_SIDE_PLAYER)
            gBattleResults.lastUsedMovePlayer = gCurrentMove;
        else
            gBattleResults.lastUsedMoveOpponent = gCurrentMove;
    }

    // Set dynamic move type.
    SetTypeBeforeUsingMove(gChosenMove, gBattlerAttacker);
    GET_MOVE_TYPE(gChosenMove, moveType);

    // choose target
    side = GetBattlerSide(gBattlerAttacker) ^ BIT_SIDE;
    if (gSideTimers[side].followmeTimer != 0
        && gBattleMoves[gCurrentMove].target == MOVE_TARGET_SELECTED
        && GetBattlerSide(gBattlerAttacker) != GetBattlerSide(gSideTimers[side].followmeTarget)
        && gBattleMons[gSideTimers[side].followmeTarget].hp != 0
		&& gCurrentMove != MOVE_SNIPE_SHOT)
    {
        gBattlerTarget = gSideTimers[side].followmeTarget;
    }
    else if ((gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
             && gSideTimers[side].followmeTimer == 0
             && (gBattleMoves[gCurrentMove].power != 0 || gBattleMoves[gCurrentMove].target != MOVE_TARGET_USER)
             && ((gBattleMons[*(gBattleStruct->moveTarget + gBattlerAttacker)].ability != ABILITY_LIGHTNING_ROD && moveType == TYPE_ELECTRIC)
                 || (gBattleMons[*(gBattleStruct->moveTarget + gBattlerAttacker)].ability != ABILITY_STORM_DRAIN && moveType == TYPE_WATER)
                )
             )
    {
        side = GetBattlerSide(gBattlerAttacker);
        for (gActiveBattler = 0; gActiveBattler < gBattlersCount; gActiveBattler++)
        {
            if (side != GetBattlerSide(gActiveBattler)
                && *(gBattleStruct->moveTarget + gBattlerAttacker) != gActiveBattler
                && ((GetBattlerAbility(gActiveBattler) == ABILITY_LIGHTNING_ROD && moveType == TYPE_ELECTRIC)
                    || (GetBattlerAbility(gActiveBattler) == ABILITY_STORM_DRAIN && moveType == TYPE_WATER)
                   )
                && GetBattlerTurnOrderNum(gActiveBattler) < var
				&& gCurrentMove != MOVE_SNIPE_SHOT)
            {
                var = GetBattlerTurnOrderNum(gActiveBattler);
            }
        }
        if (var == 4)
        {
            if (gBattleMoves[gChosenMove].target & MOVE_TARGET_RANDOM)
            {
                if (GetBattlerSide(gBattlerAttacker) == B_SIDE_PLAYER)
                {
                    if (Random() & 1)
                        gBattlerTarget = GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT);
                    else
                        gBattlerTarget = GetBattlerAtPosition(B_POSITION_OPPONENT_RIGHT);
                }
                else
                {
                    if (Random() & 1)
                        gBattlerTarget = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
                    else
                        gBattlerTarget = GetBattlerAtPosition(B_POSITION_PLAYER_RIGHT);
                }
            }
            else if (gBattleMoves[gChosenMove].target & MOVE_TARGET_FOES_AND_ALLY)
            {
                for (gBattlerTarget = 0; gBattlerTarget < gBattlersCount; gBattlerTarget++)
                {
                    if (gBattlerTarget == gBattlerAttacker)
                        continue;
                    if (IsBattlerAlive(gBattlerTarget))
                        break;
                }
            }
            else
            {
                gBattlerTarget = *(gBattleStruct->moveTarget + gBattlerAttacker);
            }

            if (!IsBattlerAlive(gBattlerTarget))
            {
                if (GetBattlerSide(gBattlerAttacker) != GetBattlerSide(gBattlerTarget))
                {
                    gBattlerTarget = GetBattlerAtPosition(GetBattlerPosition(gBattlerTarget) ^ BIT_FLANK);
                }
                else
                {
                    gBattlerTarget = GetBattlerAtPosition(GetBattlerPosition(gBattlerAttacker) ^ BIT_SIDE);
                    if (!IsBattlerAlive(gBattlerTarget))
                        gBattlerTarget = GetBattlerAtPosition(GetBattlerPosition(gBattlerTarget) ^ BIT_FLANK);
                }
            }
        }
        else
        {
            gActiveBattler = gBattlerByTurnOrder[var];
            RecordAbilityBattle(gActiveBattler, gBattleMons[gActiveBattler].ability);
            if (gBattleMons[gActiveBattler].ability == ABILITY_LIGHTNING_ROD)
                gSpecialStatuses[gActiveBattler].lightningRodRedirected = 1;
            else if (gBattleMons[gActiveBattler].ability == ABILITY_STORM_DRAIN)
                gSpecialStatuses[gActiveBattler].stormDrainRedirected = 1;
            gBattlerTarget = gActiveBattler;
        }
    }
    else if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE
             && gBattleMoves[gChosenMove].target & MOVE_TARGET_RANDOM)
    {
        if (GetBattlerSide(gBattlerAttacker) == B_SIDE_PLAYER)
        {
            if (Random() & 1)
                gBattlerTarget = GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT);
            else
                gBattlerTarget = GetBattlerAtPosition(B_POSITION_OPPONENT_RIGHT);
        }
        else
        {
            if (Random() & 1)
                gBattlerTarget = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
            else
                gBattlerTarget = GetBattlerAtPosition(B_POSITION_PLAYER_RIGHT);
        }

        if (gAbsentBattlerFlags & gBitTable[gBattlerTarget]
            && GetBattlerSide(gBattlerAttacker) != GetBattlerSide(gBattlerTarget))
        {
            gBattlerTarget = GetBattlerAtPosition(GetBattlerPosition(gBattlerTarget) ^ BIT_FLANK);
        }
    }
    else if (gBattleMoves[gChosenMove].target == MOVE_TARGET_ALLY)
    {
        if (IsBattlerAlive(BATTLE_PARTNER(gBattlerAttacker)))
            gBattlerTarget = BATTLE_PARTNER(gBattlerAttacker);
        else
            gBattlerTarget = gBattlerAttacker;
    }
    else if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE
             && gBattleMoves[gChosenMove].target == MOVE_TARGET_FOES_AND_ALLY)
    {
        for (gBattlerTarget = 0; gBattlerTarget < gBattlersCount; gBattlerTarget++)
        {
            if (gBattlerTarget == gBattlerAttacker)
                continue;
            if (IsBattlerAlive(gBattlerTarget))
                break;
        }
    }
    else
    {
        gBattlerTarget = *(gBattleStruct->moveTarget + gBattlerAttacker);
        if (!IsBattlerAlive(gBattlerTarget))
        {
            if (GetBattlerSide(gBattlerAttacker) != GetBattlerSide(gBattlerTarget))
            {
                gBattlerTarget = GetBattlerAtPosition(GetBattlerPosition(gBattlerTarget) ^ BIT_FLANK);
            }
            else
            {
                gBattlerTarget = GetBattlerAtPosition(GetBattlerPosition(gBattlerAttacker) ^ BIT_SIDE);
                if (!IsBattlerAlive(gBattlerTarget))
                    gBattlerTarget = GetBattlerAtPosition(GetBattlerPosition(gBattlerTarget) ^ BIT_FLANK);
            }
        }
    }

    // Choose battlescript.
    if (gBattleTypeFlags & BATTLE_TYPE_PALACE
        && gProtectStructs[gBattlerAttacker].palaceUnableToUseMove)
    {
        if (gBattleMons[gBattlerAttacker].hp == 0)
        {
            gCurrentActionFuncId = B_ACTION_FINISHED;
            return;
        }
        else if (gPalaceSelectionBattleScripts[gBattlerAttacker] != NULL)
        {
            gBattleCommunication[MULTISTRING_CHOOSER] = 4;
            gBattlescriptCurrInstr = gPalaceSelectionBattleScripts[gBattlerAttacker];
            gPalaceSelectionBattleScripts[gBattlerAttacker] = NULL;
        }
        else
        {
            gBattleCommunication[MULTISTRING_CHOOSER] = 4;
            gBattlescriptCurrInstr = BattleScript_MoveUsedLoafingAround;
        }
    }
    else
    {
        gBattlescriptCurrInstr = gBattleScriptsForMoveEffects[gBattleMoves[gCurrentMove].effect];
    }

    if (gBattleTypeFlags & BATTLE_TYPE_ARENA)
        BattleArena_AddMindPoints(gBattlerAttacker);

    // Record HP of each battler
    for (i = 0; i < MAX_BATTLERS_COUNT; i++)
        gBattleStruct->hpBefore[i] = gBattleMons[i].hp;

    gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;
}

void HandleAction_Switch(void)
{
    gBattlerAttacker = gBattlerByTurnOrder[gCurrentTurnActionNumber];
    gBattle_BG0_X = 0;
    gBattle_BG0_Y = 0;
    gActionSelectionCursor[gBattlerAttacker] = 0;
    gMoveSelectionCursor[gBattlerAttacker] = 0;

    PREPARE_MON_NICK_BUFFER(gBattleTextBuff1, gBattlerAttacker, *(gBattleStruct->field_58 + gBattlerAttacker))

    gBattleScripting.battler = gBattlerAttacker;
    gBattlescriptCurrInstr = BattleScript_ActionSwitch;
    gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;

    if (gBattleResults.playerSwitchesCounter < 255)
        gBattleResults.playerSwitchesCounter++;

    UndoFormChange(gBattlerPartyIndexes[gBattlerAttacker], GetBattlerSide(gBattlerAttacker));
}

void HandleAction_UseItem(void)
{
    gBattlerAttacker = gBattlerTarget = gBattlerByTurnOrder[gCurrentTurnActionNumber];
    gBattle_BG0_X = 0;
    gBattle_BG0_Y = 0;
    ClearFuryCutterDestinyBondGrudge(gBattlerAttacker);

    gLastUsedItem = gBattleResources->bufferB[gBattlerAttacker][1] | (gBattleResources->bufferB[gBattlerAttacker][2] << 8);

    if (gLastUsedItem <= LAST_BALL) // is ball
    {
		if(gLastUsedItem != ITEM_MASTER_BALL)
        gBattlescriptCurrInstr = gBattlescriptsForBallThrow[gLastUsedItem];
		else{
			gLastUsedItem = ITEM_POKE_BALL;
			gBattlescriptCurrInstr = gBattlescriptsForBallThrow[gLastUsedItem];
		}
    }
    else if (gLastUsedItem == ITEM_POKE_DOLL || gLastUsedItem == ITEM_FLUFFY_TAIL)
    {
        gBattlescriptCurrInstr = gBattlescriptsForRunningByItem[0];
    }
    else if (GetBattlerSide(gBattlerAttacker) == B_SIDE_PLAYER)
    {
        gBattlescriptCurrInstr = gBattlescriptsForUsingItem[0];
    }
    else
    {
        gBattleScripting.battler = gBattlerAttacker;

        switch (*(gBattleStruct->AI_itemType + (gBattlerAttacker >> 1)))
        {
        case AI_ITEM_FULL_RESTORE:
        case AI_ITEM_HEAL_HP:
            break;
        case AI_ITEM_CURE_CONDITION:
            gBattleCommunication[MULTISTRING_CHOOSER] = 0;
            if (*(gBattleStruct->AI_itemFlags + gBattlerAttacker / 2) & 1)
            {
                if (*(gBattleStruct->AI_itemFlags + gBattlerAttacker / 2) & 0x3E)
                    gBattleCommunication[MULTISTRING_CHOOSER] = 5;
            }
            else
            {
                while (!(*(gBattleStruct->AI_itemFlags + gBattlerAttacker / 2) & 1))
                {
                    *(gBattleStruct->AI_itemFlags + gBattlerAttacker / 2) >>= 1;
                    gBattleCommunication[MULTISTRING_CHOOSER]++;
                }
            }
            break;
        case AI_ITEM_X_STAT:
            gBattleCommunication[MULTISTRING_CHOOSER] = 4;
            if (*(gBattleStruct->AI_itemFlags + (gBattlerAttacker >> 1)) & 0x80)
            {
                gBattleCommunication[MULTISTRING_CHOOSER] = 5;
            }
            else
            {
                PREPARE_STAT_BUFFER(gBattleTextBuff1, STAT_ATK)
                PREPARE_STRING_BUFFER(gBattleTextBuff2, CHAR_X)

                while (!((*(gBattleStruct->AI_itemFlags + (gBattlerAttacker >> 1))) & 1))
                {
                    *(gBattleStruct->AI_itemFlags + gBattlerAttacker / 2) >>= 1;
                    gBattleTextBuff1[2]++;
                }

                gBattleScripting.animArg1 = gBattleTextBuff1[2] + 14;
                gBattleScripting.animArg2 = 0;
            }
            break;
        case AI_ITEM_GUARD_SPECS:
            if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
                gBattleCommunication[MULTISTRING_CHOOSER] = 2;
            else
                gBattleCommunication[MULTISTRING_CHOOSER] = 0;
            break;
        }

        gBattlescriptCurrInstr = gBattlescriptsForUsingItem[*(gBattleStruct->AI_itemType + gBattlerAttacker / 2)];
    }
    gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;
}

bool8 TryRunFromBattle(u8 battler)
{
    bool8 effect = FALSE;
    u8 holdEffect;
    u8 pyramidMultiplier;
    u8 speedVar;

    if (gBattleMons[battler].item == ITEM_ENIGMA_BERRY)
        holdEffect = gEnigmaBerries[battler].holdEffect;
    else
        holdEffect = ItemId_GetHoldEffect(gBattleMons[battler].item);

    gPotentialItemEffectBattler = battler;

    if (holdEffect == HOLD_EFFECT_CAN_ALWAYS_RUN)
    {
        gLastUsedItem = gBattleMons[battler].item;
        gProtectStructs[battler].fleeFlag = 1;
        effect++;
    }
    else if (gBattleMons[battler].ability == ABILITY_RUN_AWAY)
    {
        if (InBattlePyramid())
        {
            gBattleStruct->runTries++;
            pyramidMultiplier = GetPyramidRunMultiplier();
            speedVar = (gBattleMons[battler].speed * pyramidMultiplier) / (gBattleMons[BATTLE_OPPOSITE(battler)].speed) + (gBattleStruct->runTries * 30);
            if (speedVar > (Random() & 0xFF))
            {
                gLastUsedAbility = ABILITY_RUN_AWAY;
                gProtectStructs[battler].fleeFlag = 2;
                effect++;
            }
        }
        else
        {
            gLastUsedAbility = ABILITY_RUN_AWAY;
            gProtectStructs[battler].fleeFlag = 2;
            effect++;
        }
    }
    else if (gBattleTypeFlags & (BATTLE_TYPE_FRONTIER | BATTLE_TYPE_TRAINER_HILL) && gBattleTypeFlags & BATTLE_TYPE_TRAINER)
    {
        effect++;
    }
    else
    {
        u8 runningFromBattler = BATTLE_OPPOSITE(battler);
        if (!IsBattlerAlive(runningFromBattler))
            runningFromBattler |= BIT_FLANK;

        if (InBattlePyramid())
        {
            pyramidMultiplier = GetPyramidRunMultiplier();
            speedVar = (gBattleMons[battler].speed * pyramidMultiplier) / (gBattleMons[runningFromBattler].speed) + (gBattleStruct->runTries * 30);
            if (speedVar > (Random() & 0xFF))
                effect++;
        }
        else if (gBattleMons[battler].speed < gBattleMons[runningFromBattler].speed)
        {
            speedVar = (gBattleMons[battler].speed * 128) / (gBattleMons[runningFromBattler].speed) + (gBattleStruct->runTries * 30);
            if (speedVar > (Random() & 0xFF))
                effect++;
        }
        else // same speed or faster
        {
            effect++;
        }

        gBattleStruct->runTries++;
    }

    if (effect)
    {
        gCurrentTurnActionNumber = gBattlersCount;
        gBattleOutcome = B_OUTCOME_RAN;
    }

    return effect;
}

void HandleAction_Run(void)
{
    gBattlerAttacker = gBattlerByTurnOrder[gCurrentTurnActionNumber];

    if (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_x2000000))
    {
        gCurrentTurnActionNumber = gBattlersCount;

        for (gActiveBattler = 0; gActiveBattler < gBattlersCount; gActiveBattler++)
        {
            if (GetBattlerSide(gActiveBattler) == B_SIDE_PLAYER)
            {
                if (gChosenActionByBattler[gActiveBattler] == B_ACTION_RUN)
                    gBattleOutcome |= B_OUTCOME_LOST;
            }
            else
            {
                if (gChosenActionByBattler[gActiveBattler] == B_ACTION_RUN)
                    gBattleOutcome |= B_OUTCOME_WON;
            }
        }

        gBattleOutcome |= B_OUTCOME_LINK_BATTLE_RAN;
        gSaveBlock2Ptr->frontier.disableRecordBattle = TRUE;
    }
    else
    {
        if (GetBattlerSide(gBattlerAttacker) == B_SIDE_PLAYER)
        {
            if (!TryRunFromBattle(gBattlerAttacker)) // failed to run away
            {
                ClearFuryCutterDestinyBondGrudge(gBattlerAttacker);
                gBattleCommunication[MULTISTRING_CHOOSER] = 3;
                gBattlescriptCurrInstr = BattleScript_PrintFailedToRunString;
                gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;
            }
        }
        else
        {
            if (!CanBattlerEscape(gBattlerAttacker))
            {
                gBattleCommunication[MULTISTRING_CHOOSER] = 4;
                gBattlescriptCurrInstr = BattleScript_PrintFailedToRunString;
                gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;
            }
            else
            {
                gCurrentTurnActionNumber = gBattlersCount;
                gBattleOutcome = B_OUTCOME_MON_FLED;
            }
        }
    }
}

void HandleAction_WatchesCarefully(void)
{
    gBattlerAttacker = gBattlerByTurnOrder[gCurrentTurnActionNumber];
    gBattle_BG0_X = 0;
    gBattle_BG0_Y = 0;
    gBattlescriptCurrInstr = gBattlescriptsForSafariActions[0];
    gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;
}

void HandleAction_SafariZoneBallThrow(void)
{
    gBattlerAttacker = gBattlerByTurnOrder[gCurrentTurnActionNumber];
    gBattle_BG0_X = 0;
    gBattle_BG0_Y = 0;
    gNumSafariBalls--;
    gLastUsedItem = ITEM_SAFARI_BALL;
    gBattlescriptCurrInstr = gBattlescriptsForBallThrow[ITEM_SAFARI_BALL];
    gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;
}

void HandleAction_ThrowBall(void)
{
    gBattlerAttacker = gBattlerByTurnOrder[gCurrentTurnActionNumber];
    gBattle_BG0_X = 0;
    gBattle_BG0_Y = 0;
    gLastUsedItem = gSaveBlock2Ptr->lastUsedBall;
    RemoveBagItem(gLastUsedItem, 1);
    gBattlescriptCurrInstr = BattleScript_BallThrow;
    gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;
}

void HandleAction_ThrowPokeblock(void)
{
    gBattlerAttacker = gBattlerByTurnOrder[gCurrentTurnActionNumber];
    gBattle_BG0_X = 0;
    gBattle_BG0_Y = 0;
    gBattleCommunication[MULTISTRING_CHOOSER] = gBattleResources->bufferB[gBattlerAttacker][1] - 1;
    gLastUsedItem = gBattleResources->bufferB[gBattlerAttacker][2];

    if (gBattleResults.pokeblockThrows < 0xFF)
        gBattleResults.pokeblockThrows++;
    if (gBattleStruct->safariPkblThrowCounter < 3)
        gBattleStruct->safariPkblThrowCounter++;
    if (gBattleStruct->safariEscapeFactor > 1)
    {
        // BUG: The safariEscapeFactor is unintetionally able to become 0 (but it can not become negative!). This causes the pokeblock throw glitch.
        // To fix that change the < in the if statement below to <=. 
        if (gBattleStruct->safariEscapeFactor <= sPkblToEscapeFactor[gBattleStruct->safariPkblThrowCounter][gBattleCommunication[MULTISTRING_CHOOSER]])
            gBattleStruct->safariEscapeFactor = 1;
        else
            gBattleStruct->safariEscapeFactor -= sPkblToEscapeFactor[gBattleStruct->safariPkblThrowCounter][gBattleCommunication[MULTISTRING_CHOOSER]];
    }

    gBattlescriptCurrInstr = gBattlescriptsForSafariActions[2];
    gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;
}

void HandleAction_GoNear(void)
{
    gBattlerAttacker = gBattlerByTurnOrder[gCurrentTurnActionNumber];
    gBattle_BG0_X = 0;
    gBattle_BG0_Y = 0;

    gBattleStruct->safariCatchFactor += sGoNearCounterToCatchFactor[gBattleStruct->safariGoNearCounter];
    if (gBattleStruct->safariCatchFactor > 20)
        gBattleStruct->safariCatchFactor = 20;

    gBattleStruct->safariEscapeFactor += sGoNearCounterToEscapeFactor[gBattleStruct->safariGoNearCounter];
    if (gBattleStruct->safariEscapeFactor > 20)
        gBattleStruct->safariEscapeFactor = 20;

    if (gBattleStruct->safariGoNearCounter < 3)
    {
        gBattleStruct->safariGoNearCounter++;
        gBattleCommunication[MULTISTRING_CHOOSER] = 0;
    }
    else
    {
        gBattleCommunication[MULTISTRING_CHOOSER] = 1; // Can't get closer.
    }
    gBattlescriptCurrInstr = gBattlescriptsForSafariActions[1];
    gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;
}

void HandleAction_SafariZoneRun(void)
{
    gBattlerAttacker = gBattlerByTurnOrder[gCurrentTurnActionNumber];
    PlaySE(SE_FLEE);
    gCurrentTurnActionNumber = gBattlersCount;
    gBattleOutcome = B_OUTCOME_RAN;
}

void HandleAction_WallyBallThrow(void)
{
    gBattlerAttacker = gBattlerByTurnOrder[gCurrentTurnActionNumber];
    gBattle_BG0_X = 0;
    gBattle_BG0_Y = 0;

    PREPARE_MON_NICK_BUFFER(gBattleTextBuff1, gBattlerAttacker, gBattlerPartyIndexes[gBattlerAttacker])

    gBattlescriptCurrInstr = gBattlescriptsForSafariActions[3];
    gCurrentActionFuncId = B_ACTION_EXEC_SCRIPT;
    gActionsByTurnOrder[1] = B_ACTION_FINISHED;
}

void HandleAction_TryFinish(void)
{
    if (!HandleFaintedMonActions())
    {
        gBattleStruct->faintedActionsState = 0;
        gCurrentActionFuncId = B_ACTION_FINISHED;
    }
}

void HandleAction_NothingIsFainted(void)
{
    gCurrentTurnActionNumber++;
    gCurrentActionFuncId = gActionsByTurnOrder[gCurrentTurnActionNumber];
    gHitMarker &= ~(HITMARKER_DESTINYBOND | HITMARKER_IGNORE_SUBSTITUTE | HITMARKER_ATTACKSTRING_PRINTED
                    | HITMARKER_NO_PPDEDUCT | HITMARKER_IGNORE_SAFEGUARD | HITMARKER_x100000
                    | HITMARKER_OBEYS | HITMARKER_x10 | HITMARKER_SYNCHRONISE_EFFECT
                    | HITMARKER_CHARGING | HITMARKER_x4000000);
}

void HandleAction_ActionFinished(void)
{
    *(gBattleStruct->monToSwitchIntoId + gBattlerByTurnOrder[gCurrentTurnActionNumber]) = 6;
    gCurrentTurnActionNumber++;
    gCurrentActionFuncId = gActionsByTurnOrder[gCurrentTurnActionNumber];
    SpecialStatusesClear();
    gHitMarker &= ~(HITMARKER_DESTINYBOND | HITMARKER_IGNORE_SUBSTITUTE | HITMARKER_ATTACKSTRING_PRINTED
                    | HITMARKER_NO_PPDEDUCT | HITMARKER_IGNORE_SAFEGUARD | HITMARKER_x100000
                    | HITMARKER_OBEYS | HITMARKER_x10 | HITMARKER_SYNCHRONISE_EFFECT
                    | HITMARKER_CHARGING | HITMARKER_x4000000);

    gCurrentMove = 0;
    gBattleMoveDamage = 0;
    gMoveResultFlags = 0;
    gBattleScripting.animTurn = 0;
    gBattleScripting.animTargetsHit = 0;
    gLastLandedMoves[gBattlerAttacker] = 0;
    gLastHitByType[gBattlerAttacker] = 0;
    gBattleStruct->dynamicMoveType = 0;
    gBattleScripting.moveendState = 0;
    gBattleScripting.moveendState = 0;
    gBattleCommunication[3] = 0;
    gBattleCommunication[4] = 0;
    gBattleScripting.multihitMoveEffect = 0;
    gBattleResources->battleScriptsStack->size = 0;
}

// rom const data

static const u8 sAbilitiesAffectedByMoldBreaker[] =
{
    [ABILITY_BATTLE_ARMOR] = 1,
    [ABILITY_CLEAR_BODY] = 1,
    [ABILITY_DAMP] = 1,
    [ABILITY_DRY_SKIN] = 1,
    [ABILITY_FILTER] = 1,
    [ABILITY_FLASH_FIRE] = 1,
    [ABILITY_FLOWER_GIFT] = 1,
    [ABILITY_HEATPROOF] = 1,
    [ABILITY_HYPER_CUTTER] = 1,
    [ABILITY_IMMUNITY] = 1,
    [ABILITY_INNER_FOCUS] = 1,
    [ABILITY_INSOMNIA] = 1,
    [ABILITY_KEEN_EYE] = 1,
    [ABILITY_LEAF_GUARD] = 1,
    [ABILITY_LEVITATE] = 1,
    [ABILITY_LIGHTNING_ROD] = 1,
    [ABILITY_LIMBER] = 1,
    [ABILITY_MAGMA_ARMOR] = 1,
    [ABILITY_MARVEL_SCALE] = 1,
    [ABILITY_MOTOR_DRIVE] = 1,
    [ABILITY_OBLIVIOUS] = 1,
    [ABILITY_OWN_TEMPO] = 1,
    [ABILITY_SAND_VEIL] = 1,
    [ABILITY_SHELL_ARMOR] = 1,
    [ABILITY_SHIELD_DUST] = 1,
    [ABILITY_SIMPLE] = 1,
    [ABILITY_SNOW_CLOAK] = 1,
    [ABILITY_SOLID_ROCK] = 1,
    [ABILITY_SOUNDPROOF] = 1,
    [ABILITY_STICKY_HOLD] = 1,
    [ABILITY_STORM_DRAIN] = 1,
    [ABILITY_STURDY] = 1,
    [ABILITY_SUCTION_CUPS] = 1,
    [ABILITY_TANGLED_FEET] = 1,
    [ABILITY_THICK_FAT] = 1,
    [ABILITY_UNAWARE] = 1,
    [ABILITY_VITAL_SPIRIT] = 1,
    [ABILITY_VOLT_ABSORB] = 1,
    [ABILITY_WATER_ABSORB] = 1,
    [ABILITY_STEEL_EATER] = 1,
    [ABILITY_EARTH_EATER] = 1,
    [ABILITY_INSECT_EATER] = 1,
    [ABILITY_WATER_VEIL] = 1,
    [ABILITY_WHITE_SMOKE] = 1,
    [ABILITY_WONDER_GUARD] = 1,
    [ABILITY_BIG_PECKS] = 1,
    [ABILITY_CONTRARY] = 1,
    [ABILITY_FRIEND_GUARD] = 1,
    [ABILITY_HEAVY_METAL] = 1,
    [ABILITY_LIGHT_METAL] = 1,
    [ABILITY_MAGIC_BOUNCE] = 1,
    [ABILITY_MULTISCALE] = 1,
    [ABILITY_SAP_SIPPER] = 1,
    [ABILITY_TELEPATHY] = 1,
    [ABILITY_WONDER_SKIN] = 1,
    [ABILITY_AROMA_VEIL] = 1,
    [ABILITY_BULLETPROOF] = 1,
    [ABILITY_FLOWER_VEIL] = 1,
    [ABILITY_FUR_COAT] = 1,
    [ABILITY_OVERCOAT] = 1,
    [ABILITY_SWEET_VEIL] = 1,
    [ABILITY_DAZZLING] = 1,
    [ABILITY_DISGUISE] = 1,
    [ABILITY_FLUFFY] = 1,
    [ABILITY_QUEENLY_MAJESTY] = 1,
    [ABILITY_WATER_BUBBLE] = 1,
};

static const u8 sAbilitiesNotTraced[ABILITIES_COUNT] =
{
    [ABILITY_BATTLE_BOND] = 1,
    [ABILITY_COMATOSE] = 1,
    [ABILITY_DISGUISE] = 1,
    [ABILITY_FLOWER_GIFT] = 1,
    [ABILITY_FORECAST] = 1,
    [ABILITY_ILLUSION] = 1,
    [ABILITY_IMPOSTER] = 1,
    [ABILITY_MULTITYPE] = 1,
    [ABILITY_NONE] = 1,
    [ABILITY_POWER_CONSTRUCT] = 1,
    [ABILITY_POWER_OF_ALCHEMY] = 1,
    [ABILITY_RECEIVER] = 1,
    [ABILITY_RKS_SYSTEM] = 1,
    [ABILITY_SCHOOLING] = 1,
    [ABILITY_SHIELDS_DOWN] = 1,
    [ABILITY_STANCE_CHANGE] = 1,
    [ABILITY_TRACE] = 1,
    [ABILITY_ZEN_MODE] = 1,
};

static const u8 sHoldEffectToType[][2] =
{
    {HOLD_EFFECT_BUG_POWER, TYPE_BUG},
    {HOLD_EFFECT_STEEL_POWER, TYPE_STEEL},
    {HOLD_EFFECT_GROUND_POWER, TYPE_GROUND},
    {HOLD_EFFECT_ROCK_POWER, TYPE_ROCK},
    {HOLD_EFFECT_GRASS_POWER, TYPE_GRASS},
    {HOLD_EFFECT_DARK_POWER, TYPE_DARK},
    {HOLD_EFFECT_FIGHTING_POWER, TYPE_FIGHTING},
    {HOLD_EFFECT_ELECTRIC_POWER, TYPE_ELECTRIC},
    {HOLD_EFFECT_WATER_POWER, TYPE_WATER},
    {HOLD_EFFECT_FLYING_POWER, TYPE_FLYING},
    {HOLD_EFFECT_POISON_POWER, TYPE_POISON},
    {HOLD_EFFECT_ICE_POWER, TYPE_ICE},
    {HOLD_EFFECT_GHOST_POWER, TYPE_GHOST},
    {HOLD_EFFECT_PSYCHIC_POWER, TYPE_PSYCHIC},
    {HOLD_EFFECT_FIRE_POWER, TYPE_FIRE},
    {HOLD_EFFECT_DRAGON_POWER, TYPE_DRAGON},
    {HOLD_EFFECT_NORMAL_POWER, TYPE_NORMAL},
    {HOLD_EFFECT_FAIRY_POWER, TYPE_FAIRY},
};

// percent in UQ_4_12 format
static const u16 sPercentToModifier[] =
{
    UQ_4_12(0.00), // 0
    UQ_4_12(0.01), // 1
    UQ_4_12(0.02), // 2
    UQ_4_12(0.03), // 3
    UQ_4_12(0.04), // 4
    UQ_4_12(0.05), // 5
    UQ_4_12(0.06), // 6
    UQ_4_12(0.07), // 7
    UQ_4_12(0.08), // 8
    UQ_4_12(0.09), // 9
    UQ_4_12(0.10), // 10
    UQ_4_12(0.11), // 11
    UQ_4_12(0.12), // 12
    UQ_4_12(0.13), // 13
    UQ_4_12(0.14), // 14
    UQ_4_12(0.15), // 15
    UQ_4_12(0.16), // 16
    UQ_4_12(0.17), // 17
    UQ_4_12(0.18), // 18
    UQ_4_12(0.19), // 19
    UQ_4_12(0.20), // 20
    UQ_4_12(0.21), // 21
    UQ_4_12(0.22), // 22
    UQ_4_12(0.23), // 23
    UQ_4_12(0.24), // 24
    UQ_4_12(0.25), // 25
    UQ_4_12(0.26), // 26
    UQ_4_12(0.27), // 27
    UQ_4_12(0.28), // 28
    UQ_4_12(0.29), // 29
    UQ_4_12(0.30), // 30
    UQ_4_12(0.31), // 31
    UQ_4_12(0.32), // 32
    UQ_4_12(0.33), // 33
    UQ_4_12(0.34), // 34
    UQ_4_12(0.35), // 35
    UQ_4_12(0.36), // 36
    UQ_4_12(0.37), // 37
    UQ_4_12(0.38), // 38
    UQ_4_12(0.39), // 39
    UQ_4_12(0.40), // 40
    UQ_4_12(0.41), // 41
    UQ_4_12(0.42), // 42
    UQ_4_12(0.43), // 43
    UQ_4_12(0.44), // 44
    UQ_4_12(0.45), // 45
    UQ_4_12(0.46), // 46
    UQ_4_12(0.47), // 47
    UQ_4_12(0.48), // 48
    UQ_4_12(0.49), // 49
    UQ_4_12(0.50), // 50
    UQ_4_12(0.51), // 51
    UQ_4_12(0.52), // 52
    UQ_4_12(0.53), // 53
    UQ_4_12(0.54), // 54
    UQ_4_12(0.55), // 55
    UQ_4_12(0.56), // 56
    UQ_4_12(0.57), // 57
    UQ_4_12(0.58), // 58
    UQ_4_12(0.59), // 59
    UQ_4_12(0.60), // 60
    UQ_4_12(0.61), // 61
    UQ_4_12(0.62), // 62
    UQ_4_12(0.63), // 63
    UQ_4_12(0.64), // 64
    UQ_4_12(0.65), // 65
    UQ_4_12(0.66), // 66
    UQ_4_12(0.67), // 67
    UQ_4_12(0.68), // 68
    UQ_4_12(0.69), // 69
    UQ_4_12(0.70), // 70
    UQ_4_12(0.71), // 71
    UQ_4_12(0.72), // 72
    UQ_4_12(0.73), // 73
    UQ_4_12(0.74), // 74
    UQ_4_12(0.75), // 75
    UQ_4_12(0.76), // 76
    UQ_4_12(0.77), // 77
    UQ_4_12(0.78), // 78
    UQ_4_12(0.79), // 79
    UQ_4_12(0.80), // 80
    UQ_4_12(0.81), // 81
    UQ_4_12(0.82), // 82
    UQ_4_12(0.83), // 83
    UQ_4_12(0.84), // 84
    UQ_4_12(0.85), // 85
    UQ_4_12(0.86), // 86
    UQ_4_12(0.87), // 87
    UQ_4_12(0.88), // 88
    UQ_4_12(0.89), // 89
    UQ_4_12(0.90), // 90
    UQ_4_12(0.91), // 91
    UQ_4_12(0.92), // 92
    UQ_4_12(0.93), // 93
    UQ_4_12(0.94), // 94
    UQ_4_12(0.95), // 95
    UQ_4_12(0.96), // 96
    UQ_4_12(0.97), // 97
    UQ_4_12(0.98), // 98
    UQ_4_12(0.99), // 99
    UQ_4_12(1.00), // 100
};

#define X UQ_4_12

static const u16 sTypeEffectivenessTable[NUMBER_OF_MON_TYPES][NUMBER_OF_MON_TYPES] =
{
//   normal  fight   flying  poison  ground  rock    bug     ghost   steel   mystery fire    water   grass  electric psychic ice     dragon  dark    fairy
    {X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(0.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0)}, // normal
    {X(2.0), X(1.0), X(0.5), X(0.5), X(1.0), X(2.0), X(0.5), X(0.0), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(2.0), X(1.0), X(2.0), X(0.5)}, // fight
    {X(1.0), X(2.0), X(1.0), X(1.0), X(1.0), X(0.5), X(2.0), X(1.0), X(0.5), X(1.0), X(1.0), X(1.0), X(2.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0)}, // flying
    {X(1.0), X(1.0), X(1.0), X(0.5), X(0.5), X(0.5), X(1.0), X(0.5), X(0.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0)}, // poison
    {X(1.0), X(1.0), X(0.0), X(2.0), X(1.0), X(2.0), X(0.5), X(1.0), X(2.0), X(1.0), X(2.0), X(1.0), X(0.5), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0)}, // ground
    {X(1.0), X(0.5), X(2.0), X(1.0), X(0.5), X(1.0), X(2.0), X(1.0), X(0.5), X(1.0), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(1.0), X(1.0)}, // rock
    {X(1.0), X(0.5), X(0.5), X(0.5), X(1.0), X(1.0), X(1.0), X(0.5), X(0.5), X(1.0), X(0.5), X(1.0), X(2.0), X(1.0), X(2.0), X(1.0), X(1.0), X(2.0), X(0.5)}, // bug
    #if B_STEEL_RESISTANCES >= GEN_6
    {X(0.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(1.0), X(0.5), X(1.0)}, // ghost
    #else
    {X(0.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(1.0), X(0.5), X(1.0)}, // ghost
    #endif
    {X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(1.0), X(0.5), X(1.0), X(0.5), X(0.5), X(1.0), X(0.5), X(1.0), X(2.0), X(1.0), X(1.0), X(2.0)}, // steel
    {X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0)}, // mystery
    {X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(2.0), X(1.0), X(2.0), X(1.0), X(0.5), X(0.5), X(2.0), X(1.0), X(1.0), X(2.0), X(0.5), X(1.0), X(1.0)}, // fire
    {X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(0.5), X(0.5), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(1.0)}, // water
    {X(1.0), X(1.0), X(0.5), X(0.5), X(2.0), X(2.0), X(0.5), X(1.0), X(0.5), X(1.0), X(0.5), X(2.0), X(0.5), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(1.0)}, // grass
    {X(1.0), X(1.0), X(2.0), X(1.0), X(0.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(0.5), X(0.5), X(1.0), X(1.0), X(0.5), X(1.0), X(1.0)}, // electric
    {X(1.0), X(2.0), X(1.0), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(1.0), X(0.0), X(1.0)}, // psychic
    {X(1.0), X(1.0), X(2.0), X(1.0), X(2.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(0.5), X(0.5), X(2.0), X(1.0), X(1.0), X(0.5), X(2.0), X(1.0), X(1.0)}, // ice
    {X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(0.0)}, // dragon
    #if B_STEEL_RESISTANCES >= GEN_6
    {X(1.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(1.0), X(0.5), X(0.5)}, // dark
    #else
    {X(1.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(1.0), X(0.5), X(0.5)}, // dark
    #endif
    {X(1.0), X(2.0), X(1.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(2.0), X(1.0)}, // fairy
};

static const u16 sInverseTypeEffectivenessTable[NUMBER_OF_MON_TYPES][NUMBER_OF_MON_TYPES] =
{
//   normal  fight   flying  poison  ground  rock    bug     ghost   steel   mystery fire    water   grass  electric psychic ice     dragon  dark    fairy
    {X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(2.0), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0)}, // normal
    {X(0.5), X(1.0), X(2.0), X(2.0), X(1.0), X(0.5), X(2.0), X(2.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(0.5), X(1.0), X(0.5), X(2.0)}, // fight
    {X(1.0), X(0.5), X(1.0), X(1.0), X(1.0), X(2.0), X(0.5), X(1.0), X(2.0), X(1.0), X(1.0), X(1.0), X(0.5), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0)}, // flying
    {X(1.0), X(1.0), X(1.0), X(2.0), X(2.0), X(2.0), X(1.0), X(2.0), X(2.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5)}, // poison
    {X(1.0), X(1.0), X(2.0), X(0.5), X(1.0), X(0.5), X(2.0), X(1.0), X(0.5), X(1.0), X(0.5), X(1.0), X(2.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0)}, // ground
    {X(1.0), X(2.0), X(0.5), X(1.0), X(2.0), X(1.0), X(0.5), X(1.0), X(2.0), X(1.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(1.0), X(1.0)}, // rock
    {X(1.0), X(2.0), X(2.0), X(2.0), X(1.0), X(1.0), X(1.0), X(2.0), X(2.0), X(1.0), X(2.0), X(1.0), X(0.5), X(1.0), X(0.5), X(1.0), X(1.0), X(0.5), X(2.0)}, // bug
    #if B_STEEL_RESISTANCES >= GEN_6
    {X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(1.0), X(2.0), X(1.0)}, // ghost
    #else
    {X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(1.0), X(2.0), X(1.0)}, // ghost
    #endif
    {X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(1.0), X(2.0), X(1.0), X(2.0), X(2.0), X(1.0), X(2.0), X(1.0), X(0.5), X(1.0), X(1.0), X(0.5)}, // steel
    {X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0)}, // mystery
    {X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(0.5), X(1.0), X(0.5), X(1.0), X(2.0), X(2.0), X(0.5), X(1.0), X(1.0), X(0.5), X(2.0), X(1.0), X(1.0)}, // fire
    {X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(2.0), X(2.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(1.0)}, // water
    {X(1.0), X(1.0), X(2.0), X(2.0), X(0.5), X(0.5), X(2.0), X(1.0), X(2.0), X(1.0), X(2.0), X(0.5), X(2.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(1.0)}, // grass
    {X(1.0), X(1.0), X(0.5), X(1.0), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(2.0), X(2.0), X(1.0), X(1.0), X(2.0), X(1.0), X(1.0)}, // electric
    {X(1.0), X(0.5), X(1.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(1.0), X(2.0), X(1.0)}, // psychic
    {X(1.0), X(1.0), X(0.5), X(1.0), X(0.5), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(2.0), X(2.0), X(0.5), X(1.0), X(1.0), X(2.0), X(0.5), X(1.0), X(1.0)}, // ice
    {X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(2.0)}, // dragon
    #if B_STEEL_RESISTANCES >= GEN_6
    {X(1.0), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(1.0), X(2.0), X(2.0)}, // dark
    #else
    {X(1.0), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(1.0), X(1.0), X(2.0), X(2.0)}, // dark
    #endif
    {X(1.0), X(0.5), X(1.0), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(2.0), X(1.0), X(2.0), X(1.0), X(1.0), X(1.0), X(1.0), X(1.0), X(0.5), X(0.5), X(1.0)}, // fairy
};

#undef X

// code
u8 GetBattlerForBattleScript(u8 caseId)
{
    u8 ret = 0;
    switch (caseId)
    {
    case BS_TARGET:
        ret = gBattlerTarget;
        break;
    case BS_ATTACKER:
        ret = gBattlerAttacker;
        break;
    case BS_EFFECT_BATTLER:
        ret = gEffectBattler;
        break;
    case BS_BATTLER_0:
        ret = 0;
        break;
    case BS_SCRIPTING:
        ret = gBattleScripting.battler;
        break;
    case BS_FAINTED:
        ret = gBattlerFainted;
        break;
    case 5:
        ret = gBattlerFainted;
        break;
    case 4:
    case 6:
    case 8:
    case 9:
    case BS_PLAYER1:
        ret = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
        break;
    case BS_OPPONENT1:
        ret = GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT);
        break;
    case BS_PLAYER2:
        ret = GetBattlerAtPosition(B_POSITION_PLAYER_RIGHT);
        break;
    case BS_OPPONENT2:
        ret = GetBattlerAtPosition(B_POSITION_OPPONENT_RIGHT);
        break;
    case BS_ABILITY_BATTLER:
        ret = gBattlerAbility;
        break;
    }
    return ret;
}

void PressurePPLose(u8 target, u8 attacker, u16 move)
{
    int moveIndex;

    if (gBattleMons[target].ability != ABILITY_PRESSURE)
        return;

    for (moveIndex = 0; moveIndex < MAX_MON_MOVES; moveIndex++)
    {
        if (gBattleMons[attacker].moves[moveIndex] == move)
            break;
    }

    if (moveIndex == MAX_MON_MOVES)
        return;

    if (gBattleMons[attacker].pp[moveIndex] != 0)
        gBattleMons[attacker].pp[moveIndex]--;

    if (!(gBattleMons[attacker].status2 & STATUS2_TRANSFORMED)
        && !(gDisableStructs[attacker].mimickedMoves & gBitTable[moveIndex]))
    {
        gActiveBattler = attacker;
        BtlController_EmitSetMonData(0, REQUEST_PPMOVE1_BATTLE + moveIndex, 0, 1, &gBattleMons[gActiveBattler].pp[moveIndex]);
        MarkBattlerForControllerExec(gActiveBattler);
    }
}

void PressurePPLoseOnUsingImprison(u8 attacker)
{
    int i, j;
    int imprisonPos = 4;
    u8 atkSide = GetBattlerSide(attacker);

    for (i = 0; i < gBattlersCount; i++)
    {
        if (atkSide != GetBattlerSide(i) && gBattleMons[i].ability == ABILITY_PRESSURE)
        {
            for (j = 0; j < MAX_MON_MOVES; j++)
            {
                if (gBattleMons[attacker].moves[j] == MOVE_IMPRISON)
                    break;
            }
            if (j != MAX_MON_MOVES)
            {
                imprisonPos = j;
                if (gBattleMons[attacker].pp[j] != 0)
                    gBattleMons[attacker].pp[j]--;
            }
        }
    }

    if (imprisonPos != 4
        && !(gBattleMons[attacker].status2 & STATUS2_TRANSFORMED)
        && !(gDisableStructs[attacker].mimickedMoves & gBitTable[imprisonPos]))
    {
        gActiveBattler = attacker;
        BtlController_EmitSetMonData(0, REQUEST_PPMOVE1_BATTLE + imprisonPos, 0, 1, &gBattleMons[gActiveBattler].pp[imprisonPos]);
        MarkBattlerForControllerExec(gActiveBattler);
    }
}

void PressurePPLoseOnUsingPerishSong(u8 attacker)
{
    int i, j;
    int perishSongPos = 4;

    for (i = 0; i < gBattlersCount; i++)
    {
        if (gBattleMons[i].ability == ABILITY_PRESSURE && i != attacker)
        {
            for (j = 0; j < MAX_MON_MOVES; j++)
            {
                if (gBattleMons[attacker].moves[j] == MOVE_PERISH_SONG)
                    break;
            }
            if (j != MAX_MON_MOVES)
            {
                perishSongPos = j;
                if (gBattleMons[attacker].pp[j] != 0)
                    gBattleMons[attacker].pp[j]--;
            }
        }
    }

    if (perishSongPos != MAX_MON_MOVES
        && !(gBattleMons[attacker].status2 & STATUS2_TRANSFORMED)
        && !(gDisableStructs[attacker].mimickedMoves & gBitTable[perishSongPos]))
    {
        gActiveBattler = attacker;
        BtlController_EmitSetMonData(0, REQUEST_PPMOVE1_BATTLE + perishSongPos, 0, 1, &gBattleMons[gActiveBattler].pp[perishSongPos]);
        MarkBattlerForControllerExec(gActiveBattler);
    }
}

void MarkAllBattlersForControllerExec(void) // unused
{
    int i;

    if (gBattleTypeFlags & BATTLE_TYPE_LINK)
    {
        for (i = 0; i < gBattlersCount; i++)
            gBattleControllerExecFlags |= gBitTable[i] << 0x1C;
    }
    else
    {
        for (i = 0; i < gBattlersCount; i++)
            gBattleControllerExecFlags |= gBitTable[i];
    }
}

bool32 IsBattlerMarkedForControllerExec(u8 battlerId)
{
    if (gBattleTypeFlags & BATTLE_TYPE_LINK)
        return (gBattleControllerExecFlags & (gBitTable[battlerId] << 0x1C)) != 0;
    else
        return (gBattleControllerExecFlags & (gBitTable[battlerId])) != 0;
}

void MarkBattlerForControllerExec(u8 battlerId)
{
    if (gBattleTypeFlags & BATTLE_TYPE_LINK)
        gBattleControllerExecFlags |= gBitTable[battlerId] << 0x1C;
    else
        gBattleControllerExecFlags |= gBitTable[battlerId];
}

void sub_803F850(u8 arg0)
{
    s32 i;

    for (i = 0; i < GetLinkPlayerCount(); i++)
        gBattleControllerExecFlags |= gBitTable[arg0] << (i << 2);

    gBattleControllerExecFlags &= ~(0x10000000 << arg0);
}

void CancelMultiTurnMoves(u8 battler)
{
    gBattleMons[battler].status2 &= ~(STATUS2_MULTIPLETURNS);
    gBattleMons[battler].status2 &= ~(STATUS2_LOCK_CONFUSE);
    gBattleMons[battler].status2 &= ~(STATUS2_UPROAR);
    gBattleMons[battler].status2 &= ~(STATUS2_BIDE);

    gStatuses3[battler] &= ~(STATUS3_SEMI_INVULNERABLE);

    gDisableStructs[battler].rolloutTimer = 0;
    gDisableStructs[battler].furyCutterCounter = 0;
}

bool8 WasUnableToUseMove(u8 battler)
{
    if (gProtectStructs[battler].prlzImmobility
        || gProtectStructs[battler].targetNotAffected
        || gProtectStructs[battler].usedImprisonedMove
        || gProtectStructs[battler].loveImmobility
        || gProtectStructs[battler].usedDisabledMove
        || gProtectStructs[battler].usedTauntedMove
        || gProtectStructs[battler].usedGravityPreventedMove
        || gProtectStructs[battler].usedHealBlockedMove
        || gProtectStructs[battler].flag2Unknown
        || gProtectStructs[battler].flinchImmobility
        || gProtectStructs[battler].confusionSelfDmg
        || gProtectStructs[battler].powderSelfDmg
        || gProtectStructs[battler].usedThroatChopPreventedMove)
        return TRUE;
    else
        return FALSE;
}

void PrepareStringBattle(u16 stringId, u8 battler)
{
    // Support for Contrary ability.
    // If a move attempted to raise stat - print "won't increase".
    // If a move attempted to lower stat - print "won't decrease".
    if (stringId == STRINGID_STATSWONTDECREASE && !(gBattleScripting.statChanger & STAT_BUFF_NEGATIVE))
        stringId = STRINGID_STATSWONTINCREASE;
    else if (stringId == STRINGID_STATSWONTINCREASE && gBattleScripting.statChanger & STAT_BUFF_NEGATIVE)
        stringId = STRINGID_STATSWONTDECREASE;

    else if (stringId == STRINGID_STATSWONTDECREASE2 && GetBattlerAbility(battler) == ABILITY_CONTRARY)
        stringId = STRINGID_STATSWONTINCREASE2;
    else if (stringId == STRINGID_STATSWONTINCREASE2 && GetBattlerAbility(battler) == ABILITY_CONTRARY)
        stringId = STRINGID_STATSWONTDECREASE2;

    // Check Defiant and Competitive stat raise whenever a stat is lowered.
    else if ((stringId == STRINGID_PKMNSSTATCHANGED4 || stringId == STRINGID_PKMNCUTSATTACKWITH)
              && ((GetBattlerAbility(gBattlerTarget) == ABILITY_DEFIANT && gBattleMons[gBattlerTarget].statStages[STAT_ATK] != 12)
                 || (GetBattlerAbility(gBattlerTarget) == ABILITY_COMPETITIVE && gBattleMons[gBattlerTarget].statStages[STAT_SPATK] != 12)
                 || (GetBattlerAbility(gBattlerTarget) == ABILITY_RUN_AWAY && gBattleMons[gBattlerTarget].statStages[STAT_SPEED] != 12))
              && gSpecialStatuses[gBattlerTarget].changedStatsBattlerId != BATTLE_PARTNER(gBattlerTarget)
              && gSpecialStatuses[gBattlerTarget].changedStatsBattlerId != gBattlerTarget)
    {
        gBattlerAbility = gBattlerTarget;
        BattleScriptPushCursor();
        gBattlescriptCurrInstr = BattleScript_DefiantActivates;
        switch(GetBattlerAbility(gBattlerTarget)){
            case ABILITY_DEFIANT:
                SET_STATCHANGER(STAT_ATK, 2, FALSE);
            break;
            case ABILITY_COMPETITIVE:
                SET_STATCHANGER(STAT_SPATK, 2, FALSE);
            break;
            case ABILITY_RUN_AWAY:
                SET_STATCHANGER(STAT_SPEED, 2, FALSE);
            break;
        }
    }

    gActiveBattler = battler;
    BtlController_EmitPrintString(0, stringId);
    MarkBattlerForControllerExec(gActiveBattler);
}

void ResetSentPokesToOpponentValue(void)
{
    s32 i;
    u32 bits = 0;

    gSentPokesToOpponent[0] = 0;
    gSentPokesToOpponent[1] = 0;

    for (i = 0; i < gBattlersCount; i += 2)
        bits |= gBitTable[gBattlerPartyIndexes[i]];

    for (i = 1; i < gBattlersCount; i += 2)
        gSentPokesToOpponent[(i & BIT_FLANK) >> 1] = bits;
}

void OpponentSwitchInResetSentPokesToOpponentValue(u8 battler)
{
    s32 i = 0;
    u32 bits = 0;

    if (GetBattlerSide(battler) == B_SIDE_OPPONENT)
    {
        u8 flank = ((battler & BIT_FLANK) >> 1);
        gSentPokesToOpponent[flank] = 0;

        for (i = 0; i < gBattlersCount; i += 2)
        {
            if (!(gAbsentBattlerFlags & gBitTable[i]))
                bits |= gBitTable[gBattlerPartyIndexes[i]];
        }

        gSentPokesToOpponent[flank] = bits;
    }
}

void UpdateSentPokesToOpponentValue(u8 battler)
{
    if (GetBattlerSide(battler) == B_SIDE_OPPONENT)
    {
        OpponentSwitchInResetSentPokesToOpponentValue(battler);
    }
    else
    {
        s32 i;
        for (i = 1; i < gBattlersCount; i++)
            gSentPokesToOpponent[(i & BIT_FLANK) >> 1] |= gBitTable[gBattlerPartyIndexes[battler]];
    }
}

void BattleScriptPush(const u8 *bsPtr)
{
    gBattleResources->battleScriptsStack->ptr[gBattleResources->battleScriptsStack->size++] = bsPtr;
}

void BattleScriptPushCursor(void)
{
    gBattleResources->battleScriptsStack->ptr[gBattleResources->battleScriptsStack->size++] = gBattlescriptCurrInstr;
}

void BattleScriptPop(void)
{
    gBattlescriptCurrInstr = gBattleResources->battleScriptsStack->ptr[--gBattleResources->battleScriptsStack->size];
}

static bool32 IsGravityPreventingMove(u32 move)
{
    if (!(gFieldStatuses & STATUS_FIELD_GRAVITY))
        return FALSE;

    switch (move)
    {
    case MOVE_BOUNCE:
    case MOVE_FLY:
    case MOVE_FLYING_PRESS:
    case MOVE_HI_JUMP_KICK:
    case MOVE_JUMP_KICK:
    case MOVE_MAGNET_RISE:
    case MOVE_SKY_DROP:
    case MOVE_SPLASH:
    case MOVE_TELEKINESIS:
    case MOVE_FLOATY_FALL:
        return TRUE;
    default:
        return FALSE;
    }
}

bool32 IsHealBlockPreventingMove(u32 battler, u32 move)
{
    if (!(gStatuses3[battler] & STATUS3_HEAL_BLOCK))
        return FALSE;

    switch (gBattleMoves[move].effect)
    {
    case EFFECT_ABSORB:
    case EFFECT_MORNING_SUN:
    case EFFECT_MOONLIGHT:
    case EFFECT_RESTORE_HP:
    case EFFECT_REST:
    case EFFECT_ROOST:
    case EFFECT_HEALING_WISH:
    case EFFECT_WISH:
    case EFFECT_DREAM_EATER:
        return TRUE;
    default:
        return FALSE;
    }
}

static bool32 IsBelchPreventingMove(u32 battler, u32 move)
{
    if (gBattleMoves[move].effect != EFFECT_BELCH)
        return FALSE;

    return !(gBattleStruct->ateBerry[battler & BIT_SIDE] & gBitTable[gBattlerPartyIndexes[battler]]);
}

u8 TrySetCantSelectMoveBattleScript(void)
{
    u32 limitations = 0;
    u8 moveId = gBattleResources->bufferB[gActiveBattler][2] & ~(RET_MEGA_EVOLUTION);
    u32 move = gBattleMons[gActiveBattler].moves[moveId];
    u32 holdEffect = GetBattlerHoldEffect(gActiveBattler, TRUE);
    u16 *choicedMove = &gBattleStruct->choicedMove[gActiveBattler];

    if (gDisableStructs[gActiveBattler].disabledMove == move && move != MOVE_NONE)
    {
        gBattleScripting.battler = gActiveBattler;
        gCurrentMove = move;
        if (gBattleTypeFlags & BATTLE_TYPE_PALACE)
        {
            gPalaceSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingDisabledMoveInPalace;
            gProtectStructs[gActiveBattler].palaceUnableToUseMove = 1;
        }
        else
        {
            gSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingDisabledMove;
            limitations++;
        }
    }

    if (move == gLastMoves[gActiveBattler] && move != MOVE_STRUGGLE && (gBattleMons[gActiveBattler].status2 & STATUS2_TORMENT))
    {
        CancelMultiTurnMoves(gActiveBattler);
        if (gBattleTypeFlags & BATTLE_TYPE_PALACE)
        {
            gPalaceSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingTormentedMoveInPalace;
            gProtectStructs[gActiveBattler].palaceUnableToUseMove = 1;
        }
        else
        {
            gSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingTormentedMove;
            limitations++;
        }
    }

    if (gDisableStructs[gActiveBattler].tauntTimer != 0 && gBattleMoves[move].power == 0)
    {
        gCurrentMove = move;
        if (gBattleTypeFlags & BATTLE_TYPE_PALACE)
        {
            gPalaceSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingNotAllowedMoveTauntInPalace;
            gProtectStructs[gActiveBattler].palaceUnableToUseMove = 1;
        }
        else
        {
            gSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingNotAllowedMoveTaunt;
            limitations++;
        }
    }

    if (gDisableStructs[gActiveBattler].throatChopTimer != 0 && gBattleMoves[move].flags & FLAG_SOUND)
    {
        gCurrentMove = move;
        if (gBattleTypeFlags & BATTLE_TYPE_PALACE)
        {
            gPalaceSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingNotAllowedMoveThroatChopInPalace;
            gProtectStructs[gActiveBattler].palaceUnableToUseMove = 1;
        }
        else
        {
            gSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingNotAllowedMoveThroatChop;
            limitations++;
        }
    }

    if (GetImprisonedMovesCount(gActiveBattler, move))
    {
        gCurrentMove = move;
        if (gBattleTypeFlags & BATTLE_TYPE_PALACE)
        {
            gPalaceSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingImprisonedMoveInPalace;
            gProtectStructs[gActiveBattler].palaceUnableToUseMove = 1;
        }
        else
        {
            gSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingImprisonedMove;
            limitations++;
        }
    }

    if (IsGravityPreventingMove(move))
    {
        gCurrentMove = move;
        if (gBattleTypeFlags & BATTLE_TYPE_PALACE)
        {
            gPalaceSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingNotAllowedMoveGravityInPalace;
            gProtectStructs[gActiveBattler].palaceUnableToUseMove = 1;
        }
        else
        {
            gSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingNotAllowedMoveGravity;
            limitations++;
        }
    }

    if (IsHealBlockPreventingMove(gActiveBattler, move))
    {
        gCurrentMove = move;
        if (gBattleTypeFlags & BATTLE_TYPE_PALACE)
        {
            gPalaceSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingNotAllowedMoveHealBlockInPalace;
            gProtectStructs[gActiveBattler].palaceUnableToUseMove = 1;
        }
        else
        {
            gSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingNotAllowedMoveHealBlock;
            limitations++;
        }
    }

    if (IsBelchPreventingMove(gActiveBattler, move))
    {
        gCurrentMove = move;
        if (gBattleTypeFlags & BATTLE_TYPE_PALACE)
        {
            gPalaceSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingNotAllowedBelchInPalace;
            gProtectStructs[gActiveBattler].palaceUnableToUseMove = 1;
        }
        else
        {
            gSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingNotAllowedBelch;
            limitations++;
        }
    }

    gPotentialItemEffectBattler = gActiveBattler;
    if (HOLD_EFFECT_CHOICE(holdEffect) && *choicedMove != 0 && *choicedMove != 0xFFFF && *choicedMove != move)
    {
        gCurrentMove = *choicedMove;
        gLastUsedItem = gBattleMons[gActiveBattler].item;
        if (gBattleTypeFlags & BATTLE_TYPE_PALACE)
        {
            gProtectStructs[gActiveBattler].palaceUnableToUseMove = 1;
        }
        else
        {
            gSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingNotAllowedMoveChoiceItem;
            limitations++;
        }
    }
    else if (holdEffect == HOLD_EFFECT_ASSAULT_VEST && gBattleMoves[move].power == 0)
    {
        gCurrentMove = move;
        gLastUsedItem = gBattleMons[gActiveBattler].item;
        if (gBattleTypeFlags & BATTLE_TYPE_PALACE)
        {
            gProtectStructs[gActiveBattler].palaceUnableToUseMove = 1;
        }
        else
        {
            gSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingNotAllowedMoveAssaultVest;
            limitations++;
        }
    }
	if ((GetBattlerAbility(gActiveBattler) == ABILITY_GORILLA_TACTICS || GetBattlerAbility(gActiveBattler) == ABILITY_SAGE_POWER) && *choicedMove != 0 
              && *choicedMove != 0xFFFF && *choicedMove != move)
    {
        gCurrentMove = *choicedMove;
        gLastUsedItem = gBattleMons[gActiveBattler].item;
        if (gBattleTypeFlags & BATTLE_TYPE_PALACE)
        {
            gProtectStructs[gActiveBattler].palaceUnableToUseMove = 1;
        }
        else
        {
            gSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingNotAllowedMoveGorillaTactics;
            limitations++;
        }
    }

    if (gBattleMons[gActiveBattler].pp[moveId] == 0)
    {
        if (gBattleTypeFlags & BATTLE_TYPE_PALACE)
        {
            gProtectStructs[gActiveBattler].palaceUnableToUseMove = 1;
        }
        else
        {
            gSelectionBattleScripts[gActiveBattler] = BattleScript_SelectingMoveWithNoPP;
            limitations++;
        }
    }

    return limitations;
}

u8 CheckMoveLimitations(u8 battlerId, u8 unusableMoves, u8 check)
{
    u8 holdEffect = GetBattlerHoldEffect(battlerId, TRUE);
    u16 *choicedMove = &gBattleStruct->choicedMove[battlerId];
    s32 i;

    gPotentialItemEffectBattler = battlerId;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (gBattleMons[battlerId].moves[i] == 0 && check & MOVE_LIMITATION_ZEROMOVE)
            unusableMoves |= gBitTable[i];
        else if (gBattleMons[battlerId].pp[i] == 0 && check & MOVE_LIMITATION_PP)
            unusableMoves |= gBitTable[i];
        else if (gBattleMons[battlerId].moves[i] == gDisableStructs[battlerId].disabledMove && check & MOVE_LIMITATION_DISABLED)
            unusableMoves |= gBitTable[i];
        else if (gBattleMons[battlerId].moves[i] == gLastMoves[battlerId] && check & MOVE_LIMITATION_TORMENTED && gBattleMons[battlerId].status2 & STATUS2_TORMENT)
            unusableMoves |= gBitTable[i];
        else if (gDisableStructs[battlerId].tauntTimer && check & MOVE_LIMITATION_TAUNT && gBattleMoves[gBattleMons[battlerId].moves[i]].power == 0)
            unusableMoves |= gBitTable[i];
        else if (GetImprisonedMovesCount(battlerId, gBattleMons[battlerId].moves[i]) && check & MOVE_LIMITATION_IMPRISON)
            unusableMoves |= gBitTable[i];
        else if (gDisableStructs[battlerId].encoreTimer && gDisableStructs[battlerId].encoredMove != gBattleMons[battlerId].moves[i])
            unusableMoves |= gBitTable[i];
        else if (HOLD_EFFECT_CHOICE(holdEffect) && *choicedMove != 0 && *choicedMove != 0xFFFF && *choicedMove != gBattleMons[battlerId].moves[i])
            unusableMoves |= gBitTable[i];
        else if (holdEffect == HOLD_EFFECT_ASSAULT_VEST && gBattleMoves[gBattleMons[battlerId].moves[i]].power == 0)
            unusableMoves |= gBitTable[i];
        else if (IsGravityPreventingMove(gBattleMons[battlerId].moves[i]))
            unusableMoves |= gBitTable[i];
        else if (IsHealBlockPreventingMove(battlerId, gBattleMons[battlerId].moves[i]))
            unusableMoves |= gBitTable[i];
        else if (IsBelchPreventingMove(battlerId, gBattleMons[battlerId].moves[i]))
            unusableMoves |= gBitTable[i];
        else if (gDisableStructs[battlerId].throatChopTimer && gBattleMoves[gBattleMons[battlerId].moves[i]].flags & FLAG_SOUND)
            unusableMoves |= gBitTable[i];
		else if (GetBattlerAbility(battlerId) == ABILITY_GORILLA_TACTICS && *choicedMove != 0 && *choicedMove != 0xFFFF && *choicedMove != gBattleMons[battlerId].moves[i])
            unusableMoves |= gBitTable[i];
		else if (GetBattlerAbility(battlerId) == ABILITY_SAGE_POWER && *choicedMove != 0 && *choicedMove != 0xFFFF && *choicedMove != gBattleMons[battlerId].moves[i])
            unusableMoves |= gBitTable[i];
    }
    return unusableMoves;
}

bool8 AreAllMovesUnusable(void)
{
    u8 unusable;
    unusable = CheckMoveLimitations(gActiveBattler, 0, 0xFF);

    if (unusable == 0xF) // All moves are unusable.
    {
        gProtectStructs[gActiveBattler].noValidMoves = 1;
        gSelectionBattleScripts[gActiveBattler] = BattleScript_NoMovesLeft;
    }
    else
    {
        gProtectStructs[gActiveBattler].noValidMoves = 0;
    }

    return (unusable == 0xF);
}

u8 GetImprisonedMovesCount(u8 battlerId, u16 move)
{
    s32 i;
    u8 imprisonedMoves = 0;
    u8 battlerSide = GetBattlerSide(battlerId);

    for (i = 0; i < gBattlersCount; i++)
    {
        if (battlerSide != GetBattlerSide(i) && gStatuses3[i] & STATUS3_IMPRISONED_OTHERS)
        {
            s32 j;
            for (j = 0; j < MAX_MON_MOVES; j++)
            {
                if (move == gBattleMons[i].moves[j])
                    break;
            }
            if (j < MAX_MON_MOVES)
                imprisonedMoves++;
        }
    }

    return imprisonedMoves;
}

enum
{
    ENDTURN_ORDER,
    ENDTURN_REFLECT,
    ENDTURN_LIGHT_SCREEN,
    ENDTURN_AURORA_VEIL,
    ENDTURN_MIST,
    ENDTURN_LUCKY_CHANT,
    ENDTURN_SAFEGUARD,
    ENDTURN_TAILWIND,
    ENDTURN_WISH,
    ENDTURN_RAIN,
    ENDTURN_SANDSTORM,
    ENDTURN_SUN,
    ENDTURN_HAIL,
    ENDTURN_GRAVITY,
    ENDTURN_WATER_SPORT,
    ENDTURN_MUD_SPORT,
    ENDTURN_TRICK_ROOM,
	ENDTURN_INVERSE_ROOM,
    ENDTURN_WONDER_ROOM,
    ENDTURN_MAGIC_ROOM,
    ENDTURN_ELECTRIC_TERRAIN,
    ENDTURN_MISTY_TERRAIN,
    ENDTURN_GRASSY_TERRAIN,
    ENDTURN_PSYCHIC_TERRAIN,
    ENDTURN_ION_DELUGE,
    ENDTURN_FAIRY_LOCK,
	ENDTURN_ICE_FACE,
    ENDTURN_FIELD_COUNT,
};

u8 DoFieldEndTurnEffects(void)
{
    u8 effect = 0;

    for (gBattlerAttacker = 0; gBattlerAttacker < gBattlersCount && gAbsentBattlerFlags & gBitTable[gBattlerAttacker]; gBattlerAttacker++)
    {
    }
    for (gBattlerTarget = 0; gBattlerTarget < gBattlersCount && gAbsentBattlerFlags & gBitTable[gBattlerTarget]; gBattlerTarget++)
    {
    }

    do
    {
        s32 i;
        u8 side;

        switch (gBattleStruct->turnCountersTracker)
        {
        case ENDTURN_ORDER:
            for (i = 0; i < gBattlersCount; i++)
            {
                gBattlerByTurnOrder[i] = i;
            }
            for (i = 0; i < gBattlersCount - 1; i++)
            {
                s32 j;
                for (j = i + 1; j < gBattlersCount; j++)
                {
                    if (GetWhichBattlerFaster(gBattlerByTurnOrder[i], gBattlerByTurnOrder[j], 0))
                        SwapTurnOrder(i, j);
                }
            }

            gBattleStruct->turnCountersTracker++;
            gBattleStruct->turnSideTracker = 0;
            // fall through
        case ENDTURN_REFLECT:
            while (gBattleStruct->turnSideTracker < 2)
            {
                side = gBattleStruct->turnSideTracker;
                gActiveBattler = gBattlerAttacker = gSideTimers[side].reflectBattlerId;
                if (gSideStatuses[side] & SIDE_STATUS_REFLECT)
                {
                    if (--gSideTimers[side].reflectTimer == 0)
                    {
                        gSideStatuses[side] &= ~SIDE_STATUS_REFLECT;
                        BattleScriptExecute(BattleScript_SideStatusWoreOff);
                        PREPARE_MOVE_BUFFER(gBattleTextBuff1, MOVE_REFLECT);
                        effect++;
                    }
                }
                gBattleStruct->turnSideTracker++;
                if (effect)
                    break;
            }
            if (!effect)
            {
                gBattleStruct->turnCountersTracker++;
                gBattleStruct->turnSideTracker = 0;
            }
            break;
        case ENDTURN_LIGHT_SCREEN:
            while (gBattleStruct->turnSideTracker < 2)
            {
                side = gBattleStruct->turnSideTracker;
                gActiveBattler = gBattlerAttacker = gSideTimers[side].lightscreenBattlerId;
                if (gSideStatuses[side] & SIDE_STATUS_LIGHTSCREEN)
                {
                    if (--gSideTimers[side].lightscreenTimer == 0)
                    {
                        gSideStatuses[side] &= ~SIDE_STATUS_LIGHTSCREEN;
                        BattleScriptExecute(BattleScript_SideStatusWoreOff);
                        gBattleCommunication[MULTISTRING_CHOOSER] = side;
                        PREPARE_MOVE_BUFFER(gBattleTextBuff1, MOVE_LIGHT_SCREEN);
                        effect++;
                    }
                }
                gBattleStruct->turnSideTracker++;
                if (effect)
                    break;
            }
            if (!effect)
            {
                gBattleStruct->turnCountersTracker++;
                gBattleStruct->turnSideTracker = 0;
            }
            break;
        case ENDTURN_AURORA_VEIL:
            while (gBattleStruct->turnSideTracker < 2)
            {
                side = gBattleStruct->turnSideTracker;
                gActiveBattler = gBattlerAttacker = gSideTimers[side].auroraVeilBattlerId;
                if (gSideStatuses[side] & SIDE_STATUS_AURORA_VEIL)
                {
                    if (--gSideTimers[side].auroraVeilTimer == 0)
                    {
                        gSideStatuses[side] &= ~SIDE_STATUS_AURORA_VEIL;
                        BattleScriptExecute(BattleScript_SideStatusWoreOff);
                        gBattleCommunication[MULTISTRING_CHOOSER] = side;
                        PREPARE_MOVE_BUFFER(gBattleTextBuff1, MOVE_AURORA_VEIL);
                        effect++;
                    }
                }
                gBattleStruct->turnSideTracker++;
                if (effect)
                    break;
            }
            if (!effect)
            {
                gBattleStruct->turnCountersTracker++;
                gBattleStruct->turnSideTracker = 0;
            }
            break;
        case ENDTURN_MIST:
            while (gBattleStruct->turnSideTracker < 2)
            {
                side = gBattleStruct->turnSideTracker;
                gActiveBattler = gBattlerAttacker = gSideTimers[side].mistBattlerId;
                if (gSideTimers[side].mistTimer != 0
                 && --gSideTimers[side].mistTimer == 0)
                {
                    gSideStatuses[side] &= ~SIDE_STATUS_MIST;
                    BattleScriptExecute(BattleScript_SideStatusWoreOff);
                    gBattleCommunication[MULTISTRING_CHOOSER] = side;
                    PREPARE_MOVE_BUFFER(gBattleTextBuff1, MOVE_MIST);
                    effect++;
                }
                gBattleStruct->turnSideTracker++;
                if (effect)
                    break;
            }
            if (!effect)
            {
                gBattleStruct->turnCountersTracker++;
                gBattleStruct->turnSideTracker = 0;
            }
            break;
        case ENDTURN_SAFEGUARD:
            while (gBattleStruct->turnSideTracker < 2)
            {
                side = gBattleStruct->turnSideTracker;
                gActiveBattler = gBattlerAttacker = gSideTimers[side].safeguardBattlerId;
                if (gSideStatuses[side] & SIDE_STATUS_SAFEGUARD)
                {
                    if (--gSideTimers[side].safeguardTimer == 0)
                    {
                        gSideStatuses[side] &= ~SIDE_STATUS_SAFEGUARD;
                        BattleScriptExecute(BattleScript_SafeguardEnds);
                        effect++;
                    }
                }
                gBattleStruct->turnSideTracker++;
                if (effect)
                    break;
            }
            if (!effect)
            {
                gBattleStruct->turnCountersTracker++;
                gBattleStruct->turnSideTracker = 0;
            }
            break;
        case ENDTURN_LUCKY_CHANT:
            while (gBattleStruct->turnSideTracker < 2)
            {
                side = gBattleStruct->turnSideTracker;
                gActiveBattler = gBattlerAttacker = gSideTimers[side].luckyChantBattlerId;
                if (gSideStatuses[side] & SIDE_STATUS_LUCKY_CHANT)
                {
                    if (--gSideTimers[side].luckyChantTimer == 0)
                    {
                        gSideStatuses[side] &= ~SIDE_STATUS_LUCKY_CHANT;
                        BattleScriptExecute(BattleScript_LuckyChantEnds);
                        effect++;
                    }
                }
                gBattleStruct->turnSideTracker++;
                if (effect)
                    break;
            }
            if (!effect)
            {
                gBattleStruct->turnCountersTracker++;
                gBattleStruct->turnSideTracker = 0;
            }
            break;
        case ENDTURN_TAILWIND:
            while (gBattleStruct->turnSideTracker < 2)
            {
                side = gBattleStruct->turnSideTracker;
                gActiveBattler = gBattlerAttacker = gSideTimers[side].tailwindBattlerId;
                if (gSideStatuses[side] & SIDE_STATUS_TAILWIND)
                {
                    if (--gSideTimers[side].tailwindTimer == 0)
                    {
                        gSideStatuses[side] &= ~SIDE_STATUS_TAILWIND;
                        BattleScriptExecute(BattleScript_TailwindEnds);
                        effect++;
                    }
                }
                gBattleStruct->turnSideTracker++;
                if (effect)
                    break;
            }
            if (!effect)
            {
                gBattleStruct->turnCountersTracker++;
                gBattleStruct->turnSideTracker = 0;
            }
            break;
        case ENDTURN_WISH:
            while (gBattleStruct->turnSideTracker < gBattlersCount)
            {
                gActiveBattler = gBattlerByTurnOrder[gBattleStruct->turnSideTracker];
                if (gWishFutureKnock.wishCounter[gActiveBattler] != 0
                 && --gWishFutureKnock.wishCounter[gActiveBattler] == 0
                 && gBattleMons[gActiveBattler].hp != 0)
                {
                    gBattlerTarget = gActiveBattler;
                    BattleScriptExecute(BattleScript_WishComesTrue);
                    effect++;
                }
                gBattleStruct->turnSideTracker++;
                if (effect)
                    break;
            }
            if (!effect)
            {
                gBattleStruct->turnCountersTracker++;
            }
            break;
        case ENDTURN_RAIN:
            if (gBattleWeather & WEATHER_RAIN_ANY)
            {
                if (!(gBattleWeather & WEATHER_RAIN_PERMANENT))
                {
                    if (--gWishFutureKnock.weatherDuration == 0)
                    {
                        gBattleWeather &= ~WEATHER_RAIN_TEMPORARY;
                        gBattleWeather &= ~WEATHER_RAIN_DOWNPOUR;
                        gBattleCommunication[MULTISTRING_CHOOSER] = 2;
                    }
                    else if (gBattleWeather & WEATHER_RAIN_DOWNPOUR)
                        gBattleCommunication[MULTISTRING_CHOOSER] = 1;
                    else
                        gBattleCommunication[MULTISTRING_CHOOSER] = 0;
                }
                else if (gBattleWeather & WEATHER_RAIN_DOWNPOUR)
                {
                    gBattleCommunication[MULTISTRING_CHOOSER] = 1;
                }
                else
                {
                    gBattleCommunication[MULTISTRING_CHOOSER] = 0;
                }

                BattleScriptExecute(BattleScript_RainContinuesOrEnds);
                effect++;
            }
            gBattleStruct->turnCountersTracker++;
            break;
        case ENDTURN_SANDSTORM:
            if (gBattleWeather & WEATHER_SANDSTORM_ANY)
            {
                if (!(gBattleWeather & WEATHER_SANDSTORM_PERMANENT) && --gWishFutureKnock.weatherDuration == 0)
                {
                    gBattleWeather &= ~WEATHER_SANDSTORM_TEMPORARY;
                    gBattlescriptCurrInstr = BattleScript_SandStormHailEnds;
                }
                else
                {
                    gBattlescriptCurrInstr = BattleScript_DamagingWeatherContinues;
                }

                gBattleScripting.animArg1 = B_ANIM_SANDSTORM_CONTINUES;
                gBattleCommunication[MULTISTRING_CHOOSER] = 0;
                BattleScriptExecute(gBattlescriptCurrInstr);
                effect++;
            }
            gBattleStruct->turnCountersTracker++;
            break;
        case ENDTURN_SUN:
            if (gBattleWeather & WEATHER_SUN_ANY)
            {
                if (!(gBattleWeather & WEATHER_SUN_PERMANENT) && --gWishFutureKnock.weatherDuration == 0)
                {
                    gBattleWeather &= ~WEATHER_SUN_TEMPORARY;
                    gBattlescriptCurrInstr = BattleScript_SunlightFaded;
                }
                else
                {
                    gBattlescriptCurrInstr = BattleScript_SunlightContinues;
                }

                BattleScriptExecute(gBattlescriptCurrInstr);
                effect++;
            }
            gBattleStruct->turnCountersTracker++;
            break;
        case ENDTURN_HAIL:
            if (gBattleWeather & WEATHER_HAIL_ANY)
            {
                if (!(gBattleWeather & WEATHER_HAIL_PERMANENT) && --gWishFutureKnock.weatherDuration == 0)
                {
                    gBattleWeather &= ~WEATHER_HAIL_TEMPORARY;
                    gBattlescriptCurrInstr = BattleScript_SandStormHailEnds;
					for (i = 0; i < gBattlersCount; i++)
                        gBattleResources->flags->flags[i] &= ~(RESOURCE_FLAG_ICE_FACE);
                }
                else
                {
                    gBattlescriptCurrInstr = BattleScript_DamagingWeatherContinues;
                }

                gBattleScripting.animArg1 = B_ANIM_HAIL_CONTINUES;
                gBattleCommunication[MULTISTRING_CHOOSER] = 1;
                BattleScriptExecute(gBattlescriptCurrInstr);
                effect++;
            }
            gBattleStruct->turnCountersTracker++;
            break;
        case ENDTURN_TRICK_ROOM:
            if (gFieldStatuses & STATUS_FIELD_TRICK_ROOM && --gFieldTimers.trickRoomTimer == 0)
            {
                gFieldStatuses &= ~(STATUS_FIELD_TRICK_ROOM);
                BattleScriptExecute(BattleScript_TrickRoomEnds);
                effect++;
            }
            gBattleStruct->turnCountersTracker++;
            break;
		case ENDTURN_INVERSE_ROOM:
			if (gFieldStatuses & STATUS_FIELD_INVERSE_ROOM && --gFieldTimers.inverseRoomTimer == 0)
            {
                gFieldStatuses &= ~(STATUS_FIELD_INVERSE_ROOM);
                BattleScriptExecute(BattleScript_TrickRoomEnds);
                effect++;
            }
            gBattleStruct->turnCountersTracker++;
            break;
        case ENDTURN_WONDER_ROOM:
            if (gFieldStatuses & STATUS_FIELD_WONDER_ROOM && --gFieldTimers.wonderRoomTimer == 0)
            {
                gFieldStatuses &= ~(STATUS_FIELD_WONDER_ROOM);
                BattleScriptExecute(BattleScript_WonderRoomEnds);
                effect++;
            }
            gBattleStruct->turnCountersTracker++;
            break;
        case ENDTURN_MAGIC_ROOM:
            if (gFieldStatuses & STATUS_FIELD_MAGIC_ROOM && --gFieldTimers.magicRoomTimer == 0)
            {
                gFieldStatuses &= ~(STATUS_FIELD_MAGIC_ROOM);
                BattleScriptExecute(BattleScript_MagicRoomEnds);
                effect++;
            }
            gBattleStruct->turnCountersTracker++;
            break;
        case ENDTURN_ELECTRIC_TERRAIN:
            if (gFieldStatuses & STATUS_FIELD_ELECTRIC_TERRAIN && --gFieldTimers.electricTerrainTimer == 0)
            {
                gFieldStatuses &= ~(STATUS_FIELD_ELECTRIC_TERRAIN);
                BattleScriptExecute(BattleScript_ElectricTerrainEnds);
                effect++;
            }
            gBattleStruct->turnCountersTracker++;
            break;
        case ENDTURN_MISTY_TERRAIN:
            if (gFieldStatuses & STATUS_FIELD_MISTY_TERRAIN && --gFieldTimers.mistyTerrainTimer == 0)
            {
                gFieldStatuses &= ~(STATUS_FIELD_MISTY_TERRAIN);
                BattleScriptExecute(BattleScript_MistyTerrainEnds);
                effect++;
            }
            gBattleStruct->turnCountersTracker++;
            break;
        case ENDTURN_GRASSY_TERRAIN:
            if (gFieldStatuses & STATUS_FIELD_GRASSY_TERRAIN)
            {
                if (gFieldTimers.grassyTerrainTimer == 0 || --gFieldTimers.grassyTerrainTimer == 0)
                    gFieldStatuses &= ~(STATUS_FIELD_GRASSY_TERRAIN);
                BattleScriptExecute(BattleScript_GrassyTerrainHeals);
                effect++;
            }
            gBattleStruct->turnCountersTracker++;
            break;
        case ENDTURN_PSYCHIC_TERRAIN:
            if (gFieldStatuses & STATUS_FIELD_PSYCHIC_TERRAIN && --gFieldTimers.psychicTerrainTimer == 0)
            {
                gFieldStatuses &= ~(STATUS_FIELD_PSYCHIC_TERRAIN);
                BattleScriptExecute(BattleScript_PsychicTerrainEnds);
                effect++;
            }
            gBattleStruct->turnCountersTracker++;
            break;
        case ENDTURN_WATER_SPORT:
            if (gFieldStatuses & STATUS_FIELD_WATERSPORT && --gFieldTimers.waterSportTimer == 0)
            {
                gFieldStatuses &= ~(STATUS_FIELD_WATERSPORT);
                BattleScriptExecute(BattleScript_WaterSportEnds);
                effect++;
            }
            gBattleStruct->turnCountersTracker++;
            break;
        case ENDTURN_MUD_SPORT:
            if (gFieldStatuses & STATUS_FIELD_MUDSPORT && --gFieldTimers.mudSportTimer == 0)
            {
                gFieldStatuses &= ~(STATUS_FIELD_MUDSPORT);
                BattleScriptExecute(BattleScript_MudSportEnds);
                effect++;
            }
            gBattleStruct->turnCountersTracker++;
            break;
        case ENDTURN_GRAVITY:
            if (gFieldStatuses & STATUS_FIELD_GRAVITY && --gFieldTimers.gravityTimer == 0)
            {
                gFieldStatuses &= ~(STATUS_FIELD_GRAVITY);
                BattleScriptExecute(BattleScript_GravityEnds);
                effect++;
            }
            gBattleStruct->turnCountersTracker++;
            break;
        case ENDTURN_ION_DELUGE:
            gFieldStatuses &= ~(STATUS_FIELD_ION_DELUGE);
            gBattleStruct->turnCountersTracker++;
            break;
        case ENDTURN_FAIRY_LOCK:
            if (gFieldStatuses & STATUS_FIELD_FAIRY_LOCK && --gFieldTimers.fairyLockTimer == 0)
            {
                gFieldStatuses &= ~(STATUS_FIELD_FAIRY_LOCK);
            }
            gBattleStruct->turnCountersTracker++;
            break;
		case ENDTURN_ICE_FACE:
            for (i = 0; i < gBattlersCount; i++)
            {
                if (!(gBattleResources->flags->flags[i] & RESOURCE_FLAG_ICE_FACE))
                    TryToRevertIceFace(i);
            }
            gBattleStruct->turnCountersTracker++;
            break;
        case ENDTURN_FIELD_COUNT:
            effect++;
            break;
        }
    } while (effect == 0);

    return (gBattleMainFunc != BattleTurnPassed);
}

enum
{
    ENDTURN_INGRAIN,
    ENDTURN_AQUA_RING,
    ENDTURN_ABILITIES,
    ENDTURN_ITEMS1,
    ENDTURN_LEECH_SEED,
    ENDTURN_POISON,
    ENDTURN_BAD_POISON,
    ENDTURN_BURN,
    ENDTURN_NIGHTMARES,
    ENDTURN_CURSE,
    ENDTURN_WRAP,
    ENDTURN_UPROAR,
    ENDTURN_THRASH,
    ENDTURN_FLINCH,
    ENDTURN_DISABLE,
    ENDTURN_ENCORE,
    ENDTURN_MAGNET_RISE,
    ENDTURN_TELEKINESIS,
    ENDTURN_HEALBLOCK,
    ENDTURN_EMBARGO,
    ENDTURN_LOCK_ON,
    ENDTURN_CHARGE,
    ENDTURN_LASER_FOCUS,
    ENDTURN_TAUNT,
    ENDTURN_YAWN,
    ENDTURN_ITEMS2,
    ENDTURN_ORBS,
    ENDTURN_ROOST,
    ENDTURN_ELECTRIFY,
    ENDTURN_POWDER,
    ENDTURN_THROAT_CHOP,
    ENDTURN_SLOW_START,
    ENDTURN_BATTLER_COUNT
};

// Ingrain, Leech Seed, Strength Sap and Aqua Ring
s32 GetDrainedBigRootHp(u32 battler, s32 hp)
{
    if (GetBattlerHoldEffect(battler, TRUE) == HOLD_EFFECT_BIG_ROOT)
        hp = (hp * 1300) / 1000;
    if (hp == 0)
        hp = 1;

    return hp * -1;
}

#define MAGIC_GAURD_CHECK \
if (ability == ABILITY_MAGIC_GUARD) \
{\
    RecordAbilityBattle(gActiveBattler, ability);\
    gBattleStruct->turnEffectsTracker++;\
            break;\
}

#define BURN_GUARD_CHECK \
if (ability == ABILITY_FLARE_BOOST || ability == ABILITY_HEATPROOF) \
{\
    RecordAbilityBattle(gActiveBattler, ability);\
    gBattleStruct->turnEffectsTracker++;\
            break;\
}

#define POISON_GUARD_CHECK \
if (ability == ABILITY_TOXIC_BOOST) \
{\
    RecordAbilityBattle(gActiveBattler, ability);\
    gBattleStruct->turnEffectsTracker++;\
            break;\
}


u8 DoBattlerEndTurnEffects(void)
{
    u32 ability, i, effect = 0;

    gHitMarker |= (HITMARKER_GRUDGE | HITMARKER_x20);
    while (gBattleStruct->turnEffectsBattlerId < gBattlersCount && gBattleStruct->turnEffectsTracker <= ENDTURN_BATTLER_COUNT)
    {
        gActiveBattler = gBattlerAttacker = gBattlerByTurnOrder[gBattleStruct->turnEffectsBattlerId];
        if (gAbsentBattlerFlags & gBitTable[gActiveBattler])
        {
            gBattleStruct->turnEffectsBattlerId++;
            continue;
        }

        ability = GetBattlerAbility(gActiveBattler);
        switch (gBattleStruct->turnEffectsTracker)
        {
        case ENDTURN_INGRAIN:  // ingrain
            if ((gStatuses3[gActiveBattler] & STATUS3_ROOTED)
             && !BATTLER_MAX_HP(gActiveBattler)
             && !(gStatuses3[gActiveBattler] & STATUS3_HEAL_BLOCK)
             && gBattleMons[gActiveBattler].hp != 0)
            {
                gBattleMoveDamage = GetDrainedBigRootHp(gActiveBattler, gBattleMons[gActiveBattler].maxHP / 16);
                BattleScriptExecute(BattleScript_IngrainTurnHeal);
                effect++;
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_AQUA_RING:  // aqua ring
            if ((gStatuses3[gActiveBattler] & STATUS3_AQUA_RING)
             && !BATTLER_MAX_HP(gActiveBattler)
             && !(gStatuses3[gActiveBattler] & STATUS3_HEAL_BLOCK)
             && gBattleMons[gActiveBattler].hp != 0)
            {
                gBattleMoveDamage = GetDrainedBigRootHp(gActiveBattler, gBattleMons[gActiveBattler].maxHP / 16);
                BattleScriptExecute(BattleScript_AquaRingHeal);
                effect++;
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_ABILITIES:  // end turn abilities
            if (AbilityBattleEffects(ABILITYEFFECT_ENDTURN, gActiveBattler, 0, 0, 0))
                effect++;
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_ITEMS1:  // item effects
            if (ItemBattleEffects(1, gActiveBattler, FALSE))
                effect++;
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_ITEMS2:  // item effects again
            if (ItemBattleEffects(1, gActiveBattler, TRUE))
                effect++;
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_ORBS:
            if (ItemBattleEffects(ITEMEFFECT_ORBS, gActiveBattler, FALSE))
                effect++;
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_LEECH_SEED:  // leech seed
            if ((gStatuses3[gActiveBattler] & STATUS3_LEECHSEED)
             && gBattleMons[gStatuses3[gActiveBattler] & STATUS3_LEECHSEED_BATTLER].hp != 0
             && gBattleMons[gActiveBattler].hp != 0)
            {
                MAGIC_GAURD_CHECK;

                gBattlerTarget = gStatuses3[gActiveBattler] & STATUS3_LEECHSEED_BATTLER; // Notice gBattlerTarget is actually the HP receiver.
                gBattleMoveDamage = gBattleMons[gActiveBattler].maxHP / 8;
                if (gBattleMoveDamage == 0)
                    gBattleMoveDamage = 1;
                gBattleScripting.animArg1 = gBattlerTarget;
                gBattleScripting.animArg2 = gBattlerAttacker;
                BattleScriptExecute(BattleScript_LeechSeedTurnDrain);
                effect++;
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_POISON:  // poison
            if ((gBattleMons[gActiveBattler].status1 & STATUS1_POISON)
                && gBattleMons[gActiveBattler].hp != 0)
            {
                MAGIC_GAURD_CHECK;
				POISON_GUARD_CHECK;

                if (ability == ABILITY_POISON_HEAL)
                {
                    if (!BATTLER_MAX_HP(gActiveBattler) && !(gStatuses3[gActiveBattler] & STATUS3_HEAL_BLOCK))
                    {
                        gBattleMoveDamage = gBattleMons[gActiveBattler].maxHP / 8;
                        if (gBattleMoveDamage == 0)
                            gBattleMoveDamage = 1;
                        gBattleMoveDamage *= -1;
                        BattleScriptExecute(BattleScript_PoisonHealActivates);
                        effect++;
                    }
                }
                else
                {
                    gBattleMoveDamage = gBattleMons[gActiveBattler].maxHP / 8;
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = 1;
                    BattleScriptExecute(BattleScript_PoisonTurnDmg);
                    effect++;
                }
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_BAD_POISON:  // toxic poison
            if ((gBattleMons[gActiveBattler].status1 & STATUS1_TOXIC_POISON)
                && gBattleMons[gActiveBattler].hp != 0)
            {
                MAGIC_GAURD_CHECK;
				POISON_GUARD_CHECK;

                if (ability == ABILITY_POISON_HEAL)
                {
                    if (!BATTLER_MAX_HP(gActiveBattler) && !(gStatuses3[gActiveBattler] & STATUS3_HEAL_BLOCK))
                    {
                        gBattleMoveDamage = gBattleMons[gActiveBattler].maxHP / 8;
                        if (gBattleMoveDamage == 0)
                            gBattleMoveDamage = 1;
                        gBattleMoveDamage *= -1;
                        BattleScriptExecute(BattleScript_PoisonHealActivates);
                        effect++;
                    }
                }
				else if (ability == ABILITY_GUTS || ability == ABILITY_QUICK_FEET)
                {
                    gBattleMoveDamage = gBattleMons[gActiveBattler].maxHP / 8;
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = 1;
                    BattleScriptExecute(BattleScript_PoisonTurnDmg);
                    effect++;
                }
                else
                {
                    gBattleMoveDamage = gBattleMons[gActiveBattler].maxHP / 16;
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = 1;
                    if ((gBattleMons[gActiveBattler].status1 & STATUS1_TOXIC_COUNTER) != STATUS1_TOXIC_TURN(15)) // not 16 turns
                        gBattleMons[gActiveBattler].status1 += STATUS1_TOXIC_TURN(1);
                    gBattleMoveDamage *= (gBattleMons[gActiveBattler].status1 & STATUS1_TOXIC_COUNTER) >> 8;
                    BattleScriptExecute(BattleScript_PoisonTurnDmg);
                    effect++;
                }
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_BURN:  // burn
            if ((gBattleMons[gActiveBattler].status1 & STATUS1_BURN)
                && gBattleMons[gActiveBattler].hp != 0)
            {
                MAGIC_GAURD_CHECK;
				BURN_GUARD_CHECK;

                gBattleMoveDamage = gBattleMons[gActiveBattler].maxHP / (B_BURN_DAMAGE >= GEN_7 ? 16 : 8);
                if (ability == ABILITY_HEATPROOF)
                {
                    if (gBattleMoveDamage > (gBattleMoveDamage / 2) + 1) // Record ability if the burn takes less damage than it normally would.
                        RecordAbilityBattle(gActiveBattler, ABILITY_HEATPROOF);
                    gBattleMoveDamage /= 2;
                }
                if (gBattleMoveDamage == 0)
                    gBattleMoveDamage = 1;
                BattleScriptExecute(BattleScript_BurnTurnDmg);
                effect++;
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_NIGHTMARES:  // spooky nightmares
            if ((gBattleMons[gActiveBattler].status2 & STATUS2_NIGHTMARE)
                && gBattleMons[gActiveBattler].hp != 0)
            {
                MAGIC_GAURD_CHECK;
                // R/S does not perform this sleep check, which causes the nightmare effect to
                // persist even after the affected Pokemon has been awakened by Shed Skin.
                if (gBattleMons[gActiveBattler].status1 & STATUS1_SLEEP)
                {
                    gBattleMoveDamage = gBattleMons[gActiveBattler].maxHP / 4;
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = 1;
                    BattleScriptExecute(BattleScript_NightmareTurnDmg);
                    effect++;
                }
                else
                {
                    gBattleMons[gActiveBattler].status2 &= ~STATUS2_NIGHTMARE;
                }
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_CURSE:  // curse
            if ((gBattleMons[gActiveBattler].status2 & STATUS2_CURSED)
                && gBattleMons[gActiveBattler].hp != 0)
            {
                MAGIC_GAURD_CHECK;
                gBattleMoveDamage = gBattleMons[gActiveBattler].maxHP / 4;
                if (gBattleMoveDamage == 0)
                    gBattleMoveDamage = 1;
                BattleScriptExecute(BattleScript_CurseTurnDmg);
                effect++;
            }
            gBattleStruct->turnEffectsTracker++;
            break;
		/*/case ENDTURN_OCTOLOCK:
            if (gDisableStructs[gActiveBattler].octolock 
             && !(GetBattlerAbility(gActiveBattler) == ABILITY_CLEAR_BODY 
                  || GetBattlerAbility(gActiveBattler) == ABILITY_FULL_METAL_BODY 
                  || GetBattlerAbility(gActiveBattler) == ABILITY_WHITE_SMOKE))
            {
                gBattlerTarget = gActiveBattler;
                BattleScriptExecute(BattleScript_OctolockEndTurn);
                effect++;
            }
            gBattleStruct->turnEffectsTracker++;
            break;/*/
        case ENDTURN_WRAP:  // wrap
            if ((gBattleMons[gActiveBattler].status2 & STATUS2_WRAPPED) && gBattleMons[gActiveBattler].hp != 0)
            {
                if (--gDisableStructs[gActiveBattler].wrapTurns != 0)  // damaged by wrap
                {
                    MAGIC_GAURD_CHECK;

                    gBattleScripting.animArg1 = gBattleStruct->wrappedMove[gActiveBattler];
                    gBattleScripting.animArg2 = gBattleStruct->wrappedMove[gActiveBattler] >> 8;
                    PREPARE_MOVE_BUFFER(gBattleTextBuff1, gBattleStruct->wrappedMove[gActiveBattler]);
                    gBattlescriptCurrInstr = BattleScript_WrapTurnDmg;
                    if (GetBattlerHoldEffect(gBattleStruct->wrappedBy[gActiveBattler], TRUE) == HOLD_EFFECT_BINDING_BAND)
                        gBattleMoveDamage = gBattleMons[gActiveBattler].maxHP / 8;
                    else
                        gBattleMoveDamage = gBattleMons[gActiveBattler].maxHP / 16;

                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = 1;
                }
                else  // broke free
                {
                    gBattleMons[gActiveBattler].status2 &= ~(STATUS2_WRAPPED);
                    PREPARE_MOVE_BUFFER(gBattleTextBuff1, gBattleStruct->wrappedMove[gActiveBattler]);
                    gBattlescriptCurrInstr = BattleScript_WrapEnds;
                }
                BattleScriptExecute(gBattlescriptCurrInstr);
                effect++;
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_UPROAR:  // uproar
            if (gBattleMons[gActiveBattler].status2 & STATUS2_UPROAR)
            {
                for (gBattlerAttacker = 0; gBattlerAttacker < gBattlersCount; gBattlerAttacker++)
                {
                    if ((gBattleMons[gBattlerAttacker].status1 & STATUS1_SLEEP)
                     && gBattleMons[gBattlerAttacker].ability != ABILITY_SOUNDPROOF)
                    {
                        gBattleMons[gBattlerAttacker].status1 &= ~(STATUS1_SLEEP);
                        gBattleMons[gBattlerAttacker].status2 &= ~(STATUS2_NIGHTMARE);
                        gBattleCommunication[MULTISTRING_CHOOSER] = 1;
                        BattleScriptExecute(BattleScript_MonWokeUpInUproar);
                        gActiveBattler = gBattlerAttacker;
                        BtlController_EmitSetMonData(0, REQUEST_STATUS_BATTLE, 0, 4, &gBattleMons[gActiveBattler].status1);
                        MarkBattlerForControllerExec(gActiveBattler);
                        break;
                    }
                }
                if (gBattlerAttacker != gBattlersCount)
                {
                    effect = 2;  // a pokemon was awaken
                    break;
                }
                else
                {
                    gBattlerAttacker = gActiveBattler;
                    gBattleMons[gActiveBattler].status2 -= STATUS2_UPROAR_TURN(1);  // uproar timer goes down
                    if (WasUnableToUseMove(gActiveBattler))
                    {
                        CancelMultiTurnMoves(gActiveBattler);
                        gBattleCommunication[MULTISTRING_CHOOSER] = 1;
                    }
                    else if (gBattleMons[gActiveBattler].status2 & STATUS2_UPROAR)
                    {
                        gBattleCommunication[MULTISTRING_CHOOSER] = 0;
                        gBattleMons[gActiveBattler].status2 |= STATUS2_MULTIPLETURNS;
                    }
                    else
                    {
                        gBattleCommunication[MULTISTRING_CHOOSER] = 1;
                        CancelMultiTurnMoves(gActiveBattler);
                    }
                    BattleScriptExecute(BattleScript_PrintUproarOverTurns);
                    effect = 1;
                }
            }
            if (effect != 2)
                gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_THRASH:  // thrash
            if (gBattleMons[gActiveBattler].status2 & STATUS2_LOCK_CONFUSE)
            {
                gBattleMons[gActiveBattler].status2 -= STATUS2_LOCK_CONFUSE_TURN(1);
                if (WasUnableToUseMove(gActiveBattler))
                    CancelMultiTurnMoves(gActiveBattler);
                else if (!(gBattleMons[gActiveBattler].status2 & STATUS2_LOCK_CONFUSE)
                 && (gBattleMons[gActiveBattler].status2 & STATUS2_MULTIPLETURNS))
                {
                    gBattleMons[gActiveBattler].status2 &= ~(STATUS2_MULTIPLETURNS);
                    if (!(gBattleMons[gActiveBattler].status2 & STATUS2_CONFUSION))
                    {
                        gBattleScripting.moveEffect = MOVE_EFFECT_CONFUSION | MOVE_EFFECT_AFFECTS_USER;
                        SetMoveEffect(TRUE, 0);
                        if (gBattleMons[gActiveBattler].status2 & STATUS2_CONFUSION)
                            BattleScriptExecute(BattleScript_ThrashConfuses);
                        effect++;
                    }
                }
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_FLINCH:  // reset flinch
            gBattleMons[gActiveBattler].status2 &= ~(STATUS2_FLINCHED);
            gBattleStruct->turnEffectsTracker++;
        case ENDTURN_DISABLE:  // disable
            if (gDisableStructs[gActiveBattler].disableTimer != 0)
            {
                for (i = 0; i < MAX_MON_MOVES; i++)
                {
                    if (gDisableStructs[gActiveBattler].disabledMove == gBattleMons[gActiveBattler].moves[i])
                        break;
                }
                if (i == MAX_MON_MOVES)  // pokemon does not have the disabled move anymore
                {
                    gDisableStructs[gActiveBattler].disabledMove = 0;
                    gDisableStructs[gActiveBattler].disableTimer = 0;
                }
                else if (--gDisableStructs[gActiveBattler].disableTimer == 0)  // disable ends
                {
                    gDisableStructs[gActiveBattler].disabledMove = 0;
                    BattleScriptExecute(BattleScript_DisabledNoMore);
                    effect++;
                }
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_ENCORE:  // encore
            if (gDisableStructs[gActiveBattler].encoreTimer != 0)
            {
                if (gBattleMons[gActiveBattler].moves[gDisableStructs[gActiveBattler].encoredMovePos] != gDisableStructs[gActiveBattler].encoredMove)  // pokemon does not have the encored move anymore
                {
                    gDisableStructs[gActiveBattler].encoredMove = 0;
                    gDisableStructs[gActiveBattler].encoreTimer = 0;
                }
                else if (--gDisableStructs[gActiveBattler].encoreTimer == 0
                 || gBattleMons[gActiveBattler].pp[gDisableStructs[gActiveBattler].encoredMovePos] == 0)
                {
                    gDisableStructs[gActiveBattler].encoredMove = 0;
                    gDisableStructs[gActiveBattler].encoreTimer = 0;
                    BattleScriptExecute(BattleScript_EncoredNoMore);
                    effect++;
                }
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_LOCK_ON:  // lock-on decrement
            if (gStatuses3[gActiveBattler] & STATUS3_ALWAYS_HITS)
                gStatuses3[gActiveBattler] -= STATUS3_ALWAYS_HITS_TURN(1);
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_CHARGE:  // charge
            if (gDisableStructs[gActiveBattler].chargeTimer && --gDisableStructs[gActiveBattler].chargeTimer == 0)
                gStatuses3[gActiveBattler] &= ~STATUS3_CHARGED_UP;
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_TAUNT:  // taunt
            if (gDisableStructs[gActiveBattler].tauntTimer && --gDisableStructs[gActiveBattler].tauntTimer == 0)
            {
                BattleScriptExecute(BattleScript_BufferEndTurn);
                PREPARE_MOVE_BUFFER(gBattleTextBuff1, MOVE_TAUNT);
                effect++;
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_YAWN:  // yawn
            if (gStatuses3[gActiveBattler] & STATUS3_YAWN)
            {
                gStatuses3[gActiveBattler] -= STATUS3_YAWN_TURN(1);
                if (!(gStatuses3[gActiveBattler] & STATUS3_YAWN) && !(gBattleMons[gActiveBattler].status1 & STATUS1_ANY)
                 && gBattleMons[gActiveBattler].ability != ABILITY_VITAL_SPIRIT
                 && gBattleMons[gActiveBattler].ability != ABILITY_INSOMNIA && !UproarWakeUpCheck(gActiveBattler)
                 && !IsLeafGuardProtected(gActiveBattler))
                {
                    CancelMultiTurnMoves(gActiveBattler);
                    gBattleMons[gActiveBattler].status1 |= (Random() & 3) + 2;
                    BtlController_EmitSetMonData(0, REQUEST_STATUS_BATTLE, 0, 4, &gBattleMons[gActiveBattler].status1);
                    MarkBattlerForControllerExec(gActiveBattler);
                    gEffectBattler = gActiveBattler;
                    BattleScriptExecute(BattleScript_YawnMakesAsleep);
                    effect++;
                }
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_LASER_FOCUS:
            if (gStatuses3[gActiveBattler] & STATUS3_LASER_FOCUS)
            {
                if (gDisableStructs[gActiveBattler].laserFocusTimer == 0 || --gDisableStructs[gActiveBattler].laserFocusTimer == 0)
                    gStatuses3[gActiveBattler] &= ~(STATUS3_LASER_FOCUS);
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_EMBARGO:
            if (gStatuses3[gActiveBattler] & STATUS3_EMBARGO)
            {
                if (gDisableStructs[gActiveBattler].embargoTimer == 0 || --gDisableStructs[gActiveBattler].embargoTimer == 0)
                {
                    gStatuses3[gActiveBattler] &= ~(STATUS3_EMBARGO);
                    BattleScriptExecute(BattleScript_EmbargoEndTurn);
                    effect++;
                }
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_MAGNET_RISE:
            if (gStatuses3[gActiveBattler] & STATUS3_MAGNET_RISE)
            {
                if (gDisableStructs[gActiveBattler].magnetRiseTimer == 0 || --gDisableStructs[gActiveBattler].magnetRiseTimer == 0)
                {
                    gStatuses3[gActiveBattler] &= ~(STATUS3_MAGNET_RISE);
                    BattleScriptExecute(BattleScript_BufferEndTurn);
                    PREPARE_STRING_BUFFER(gBattleTextBuff1, STRINGID_ELECTROMAGNETISM);
                    effect++;
                }
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_TELEKINESIS:
            if (gStatuses3[gActiveBattler] & STATUS3_TELEKINESIS)
            {
                if (gDisableStructs[gActiveBattler].telekinesisTimer == 0 || --gDisableStructs[gActiveBattler].telekinesisTimer == 0)
                {
                    gStatuses3[gActiveBattler] &= ~(STATUS3_TELEKINESIS);
                    BattleScriptExecute(BattleScript_TelekinesisEndTurn);
                    effect++;
                }
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_HEALBLOCK:
            if (gStatuses3[gActiveBattler] & STATUS3_HEAL_BLOCK)
            {
                if (gDisableStructs[gActiveBattler].healBlockTimer == 0 || --gDisableStructs[gActiveBattler].healBlockTimer == 0)
                {
                    gStatuses3[gActiveBattler] &= ~(STATUS3_HEAL_BLOCK);
                    BattleScriptExecute(BattleScript_BufferEndTurn);
                    PREPARE_MOVE_BUFFER(gBattleTextBuff1, MOVE_HEAL_BLOCK);
                    effect++;
                }
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_ROOST: // Return flying type.
            if (gBattleResources->flags->flags[gActiveBattler] & RESOURCE_FLAG_ROOST)
            {
                gBattleResources->flags->flags[gActiveBattler] &= ~(RESOURCE_FLAG_ROOST);
                gBattleMons[gActiveBattler].type1 = gBattleStruct->roostTypes[gActiveBattler][0];
                gBattleMons[gActiveBattler].type2 = gBattleStruct->roostTypes[gActiveBattler][1];
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_ELECTRIFY:
            gStatuses3[gActiveBattler] &= ~(STATUS3_ELECTRIFIED);
            gBattleStruct->turnEffectsTracker++;
        case ENDTURN_POWDER:
            gBattleMons[gActiveBattler].status2 &= ~(STATUS2_POWDER);
            gBattleStruct->turnEffectsTracker++;
        case ENDTURN_THROAT_CHOP:
            if (gDisableStructs[gActiveBattler].throatChopTimer && --gDisableStructs[gActiveBattler].throatChopTimer == 0)
            {
                BattleScriptExecute(BattleScript_ThroatChopEndTurn);
                effect++;
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_SLOW_START:
            if (gDisableStructs[gActiveBattler].slowStartTimer
                && --gDisableStructs[gActiveBattler].slowStartTimer == 0
                && ability == ABILITY_SLOW_START)
            {
                BattleScriptExecute(BattleScript_SlowStartEnds);
                effect++;
            }
            gBattleStruct->turnEffectsTracker++;
            break;
        case ENDTURN_BATTLER_COUNT:  // done
            gBattleStruct->turnEffectsTracker = 0;
            gBattleStruct->turnEffectsBattlerId++;
            break;
        }

        if (effect != 0)
            return effect;

    }
    gHitMarker &= ~(HITMARKER_GRUDGE | HITMARKER_x20);
    return 0;
}

bool8 HandleWishPerishSongOnTurnEnd(void)
{
    gHitMarker |= (HITMARKER_GRUDGE | HITMARKER_x20);

    switch (gBattleStruct->wishPerishSongState)
    {
    case 0:
        while (gBattleStruct->wishPerishSongBattlerId < gBattlersCount)
        {
            gActiveBattler = gBattleStruct->wishPerishSongBattlerId;
            if (gAbsentBattlerFlags & gBitTable[gActiveBattler])
            {
                gBattleStruct->wishPerishSongBattlerId++;
                continue;
            }

            gBattleStruct->wishPerishSongBattlerId++;
            if (gWishFutureKnock.futureSightCounter[gActiveBattler] != 0
             && --gWishFutureKnock.futureSightCounter[gActiveBattler] == 0
             && gBattleMons[gActiveBattler].hp != 0)
            {
                if (gWishFutureKnock.futureSightMove[gActiveBattler] == MOVE_FUTURE_SIGHT)
                    gBattleCommunication[MULTISTRING_CHOOSER] = 0;
                else
                    gBattleCommunication[MULTISTRING_CHOOSER] = 1;

                PREPARE_MOVE_BUFFER(gBattleTextBuff1, gWishFutureKnock.futureSightMove[gActiveBattler]);

                gBattlerTarget = gActiveBattler;
                gBattlerAttacker = gWishFutureKnock.futureSightAttacker[gActiveBattler];
                gSpecialStatuses[gBattlerTarget].dmg = 0xFFFF;
                gCurrentMove = gWishFutureKnock.futureSightMove[gActiveBattler];
                SetTypeBeforeUsingMove(gCurrentMove, gActiveBattler);
                BattleScriptExecute(BattleScript_MonTookFutureAttack);

                if (gWishFutureKnock.futureSightCounter[gActiveBattler] == 0
                 && gWishFutureKnock.futureSightCounter[gActiveBattler ^ BIT_FLANK] == 0)
                {
                    gSideStatuses[GET_BATTLER_SIDE(gBattlerTarget)] &= ~(SIDE_STATUS_FUTUREATTACK);
                }
                return TRUE;
            }
        }
        gBattleStruct->wishPerishSongState = 1;
        gBattleStruct->wishPerishSongBattlerId = 0;
        // fall through
    case 1:
        while (gBattleStruct->wishPerishSongBattlerId < gBattlersCount)
        {
            gActiveBattler = gBattlerAttacker = gBattlerByTurnOrder[gBattleStruct->wishPerishSongBattlerId];
            if (gAbsentBattlerFlags & gBitTable[gActiveBattler])
            {
                gBattleStruct->wishPerishSongBattlerId++;
                continue;
            }
            gBattleStruct->wishPerishSongBattlerId++;
            if (gStatuses3[gActiveBattler] & STATUS3_PERISH_SONG)
            {
                PREPARE_BYTE_NUMBER_BUFFER(gBattleTextBuff1, 1, gDisableStructs[gActiveBattler].perishSongTimer);
                if (gDisableStructs[gActiveBattler].perishSongTimer == 0)
                {
                    gStatuses3[gActiveBattler] &= ~STATUS3_PERISH_SONG;
                    gBattleMoveDamage = gBattleMons[gActiveBattler].hp;
                    gBattlescriptCurrInstr = BattleScript_PerishSongTakesLife;
                }
                else
                {
                    gDisableStructs[gActiveBattler].perishSongTimer--;
                    gBattlescriptCurrInstr = BattleScript_PerishSongCountGoesDown;
                }
                BattleScriptExecute(gBattlescriptCurrInstr);
                return TRUE;
            }
        }
        // Hm...
        {
            u8 *state = &gBattleStruct->wishPerishSongState;
            *state = 2;
            gBattleStruct->wishPerishSongBattlerId = 0;
        }
        // fall through
    case 2:
        if ((gBattleTypeFlags & BATTLE_TYPE_ARENA)
         && gBattleStruct->arenaTurnCounter == 2
         && gBattleMons[0].hp != 0 && gBattleMons[1].hp != 0)
        {
            s32 i;

            for (i = 0; i < 2; i++)
                CancelMultiTurnMoves(i);

            gBattlescriptCurrInstr = BattleScript_ArenaDoJudgment;
            BattleScriptExecute(BattleScript_ArenaDoJudgment);
            gBattleStruct->wishPerishSongState++;
            return TRUE;
        }
        break;
    }

    gHitMarker &= ~(HITMARKER_GRUDGE | HITMARKER_x20);

    return FALSE;
}

#define FAINTED_ACTIONS_MAX_CASE 7

bool8 HandleFaintedMonActions(void)
{
    if (gBattleTypeFlags & BATTLE_TYPE_SAFARI)
        return FALSE;
    do
    {
        s32 i;
        switch (gBattleStruct->faintedActionsState)
        {
        case 0:
            gBattleStruct->faintedActionsBattlerId = 0;
            gBattleStruct->faintedActionsState++;
            for (i = 0; i < gBattlersCount; i++)
            {
                if (gAbsentBattlerFlags & gBitTable[i] && !HasNoMonsToSwitch(i, 6, 6))
                    gAbsentBattlerFlags &= ~(gBitTable[i]);
            }
            // fall through
        case 1:
            do
            {
                gBattlerFainted = gBattlerTarget = gBattleStruct->faintedActionsBattlerId;
                if (gBattleMons[gBattleStruct->faintedActionsBattlerId].hp == 0
                 && !(gBattleStruct->givenExpMons & gBitTable[gBattlerPartyIndexes[gBattleStruct->faintedActionsBattlerId]])
                 && !(gAbsentBattlerFlags & gBitTable[gBattleStruct->faintedActionsBattlerId]))
                {
                    BattleScriptExecute(BattleScript_GiveExp);
                    gBattleStruct->faintedActionsState = 2;
                    return TRUE;
                }
            } while (++gBattleStruct->faintedActionsBattlerId != gBattlersCount);
            gBattleStruct->faintedActionsState = 3;
            break;
        case 2:
            OpponentSwitchInResetSentPokesToOpponentValue(gBattlerFainted);
            if (++gBattleStruct->faintedActionsBattlerId == gBattlersCount)
                gBattleStruct->faintedActionsState = 3;
            else
                gBattleStruct->faintedActionsState = 1;

            // Don't switch mons until all pokemon performed their actions or the battle's over.
            if (gBattleOutcome == 0
                && !NoAliveMonsForEitherParty()
                && gCurrentTurnActionNumber != gBattlersCount)
            {
                gAbsentBattlerFlags |= gBitTable[gBattlerFainted];
                return FALSE;
            }
            break;
        case 3:
            // Don't switch mons until all pokemon performed their actions or the battle's over.
            if (gBattleOutcome == 0
                && !NoAliveMonsForEitherParty()
                && gCurrentTurnActionNumber != gBattlersCount)
            {
                return FALSE;
            }
            gBattleStruct->faintedActionsBattlerId = 0;
            gBattleStruct->faintedActionsState++;
            // fall through
        case 4:
            do
            {
                gBattlerFainted = gBattlerTarget = gBattleStruct->faintedActionsBattlerId;
                if (gBattleMons[gBattleStruct->faintedActionsBattlerId].hp == 0
                 && !(gAbsentBattlerFlags & gBitTable[gBattleStruct->faintedActionsBattlerId]))
                {
                    BattleScriptExecute(BattleScript_HandleFaintedMon);
                    gBattleStruct->faintedActionsState = 5;
                    return TRUE;
                }
            } while (++gBattleStruct->faintedActionsBattlerId != gBattlersCount);
            gBattleStruct->faintedActionsState = 6;
            break;
        case 5:
            if (++gBattleStruct->faintedActionsBattlerId == gBattlersCount)
                gBattleStruct->faintedActionsState = 6;
            else
                gBattleStruct->faintedActionsState = 4;
            break;
        case 6:
            if (ItemBattleEffects(1, 0, TRUE))
                return TRUE;
            gBattleStruct->faintedActionsState++;
            break;
        case FAINTED_ACTIONS_MAX_CASE:
            break;
        }
    } while (gBattleStruct->faintedActionsState != FAINTED_ACTIONS_MAX_CASE);
    return FALSE;
}

void TryClearRageAndFuryCutter(void)
{
    s32 i;
    for (i = 0; i < gBattlersCount; i++)
    {
        if ((gBattleMons[i].status2 & STATUS2_RAGE) && gChosenMoveByBattler[i] != MOVE_RAGE)
            gBattleMons[i].status2 &= ~(STATUS2_RAGE);
        if (gDisableStructs[i].furyCutterCounter != 0 && gChosenMoveByBattler[i] != MOVE_FURY_CUTTER)
            gDisableStructs[i].furyCutterCounter = 0;
    }
}

enum
{
    CANCELLER_FLAGS,
    CANCELLER_ASLEEP,
    CANCELLER_FROZEN,
    CANCELLER_TRUANT,
    CANCELLER_RECHARGE,
    CANCELLER_FLINCH,
    CANCELLER_DISABLED,
    CANCELLER_GRAVITY,
    CANCELLER_HEAL_BLOCKED,
    CANCELLER_TAUNTED,
    CANCELLER_IMPRISONED,
    CANCELLER_CONFUSED,
    CANCELLER_PARALYSED,
    CANCELLER_IN_LOVE,
    CANCELLER_BIDE,
    CANCELLER_THAW,
    CANCELLER_POWDER_MOVE,
    CANCELLER_POWDER_STATUS,
    CANCELLER_THROAT_CHOP,
    CANCELLER_END,
    CANCELLER_PSYCHIC_TERRAIN,
    CANCELLER_END2,
};

u8 AtkCanceller_UnableToUseMove(void)
{
    u8 effect = 0;
    s32 *bideDmg = &gBattleScripting.bideDmg;
    do
    {
        switch (gBattleStruct->atkCancellerTracker)
        {
        case CANCELLER_FLAGS: // flags clear
            gBattleMons[gBattlerAttacker].status2 &= ~(STATUS2_DESTINY_BOND);
            gStatuses3[gBattlerAttacker] &= ~(STATUS3_GRUDGE);
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_ASLEEP: // check being asleep
            if (gBattleMons[gBattlerAttacker].status1 & STATUS1_SLEEP)
            {
                if (UproarWakeUpCheck(gBattlerAttacker))
                {
                    gBattleMons[gBattlerAttacker].status1 &= ~(STATUS1_SLEEP);
                    gBattleMons[gBattlerAttacker].status2 &= ~(STATUS2_NIGHTMARE);
                    BattleScriptPushCursor();
                    gBattleCommunication[MULTISTRING_CHOOSER] = 1;
                    gBattlescriptCurrInstr = BattleScript_MoveUsedWokeUp;
                    effect = 2;
                }
                else
                {
                    u8 toSub;
                    if (gBattleMons[gBattlerAttacker].ability == ABILITY_EARLY_BIRD)
                        toSub = 2;
                    else
                        toSub = 1;
                    if ((gBattleMons[gBattlerAttacker].status1 & STATUS1_SLEEP) < toSub)
                        gBattleMons[gBattlerAttacker].status1 &= ~(STATUS1_SLEEP);
                    else
                        gBattleMons[gBattlerAttacker].status1 -= toSub;
                    if (gBattleMons[gBattlerAttacker].status1 & STATUS1_SLEEP)
                    {
                        if (gCurrentMove != MOVE_SNORE && gCurrentMove != MOVE_SLEEP_TALK)
                        {
                            gBattlescriptCurrInstr = BattleScript_MoveUsedIsAsleep;
                            gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                            effect = 2;
                        }
                    }
                    else
                    {
                        gBattleMons[gBattlerAttacker].status2 &= ~(STATUS2_NIGHTMARE);
                        BattleScriptPushCursor();
                        gBattleCommunication[MULTISTRING_CHOOSER] = 0;
                        gBattlescriptCurrInstr = BattleScript_MoveUsedWokeUp;
                        effect = 2;
                    }
                }
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_FROZEN: // check being frozen
            if (gBattleMons[gBattlerAttacker].status1 & STATUS1_FREEZE)
            {
                if (Random() % 5)
                {
                    if (gBattleMoves[gCurrentMove].effect != EFFECT_THAW_HIT) // unfreezing via a move effect happens in case 13
                    {
                        gBattlescriptCurrInstr = BattleScript_MoveUsedIsFrozen;
                        gHitMarker |= HITMARKER_NO_ATTACKSTRING;
                    }
                    else
                    {
                        gBattleStruct->atkCancellerTracker++;
                        break;
                    }
                }
                else // unfreeze
                {
                    gBattleMons[gBattlerAttacker].status1 &= ~(STATUS1_FREEZE);
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_MoveUsedUnfroze;
                    gBattleCommunication[MULTISTRING_CHOOSER] = 0;
                }
                effect = 2;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_TRUANT: // truant
            if (gBattleMons[gBattlerAttacker].ability == ABILITY_TRUANT && 
                gDisableStructs[gBattlerAttacker].truantCounter && 
                !(gStatuses3[gBattlerAttacker] & STATUS3_GASTRO_ACID))
            {
                CancelMultiTurnMoves(gBattlerAttacker);
                gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                gBattleCommunication[MULTISTRING_CHOOSER] = 0;
                gBattlerAbility = gBattlerAttacker;
                gBattlescriptCurrInstr = BattleScript_TruantLoafingAround;
                gMoveResultFlags |= MOVE_RESULT_MISSED;
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_RECHARGE: // recharge
            if (gBattleMons[gBattlerAttacker].status2 & STATUS2_RECHARGE)
            {
                gBattleMons[gBattlerAttacker].status2 &= ~(STATUS2_RECHARGE);
                gDisableStructs[gBattlerAttacker].rechargeTimer = 0;
                CancelMultiTurnMoves(gBattlerAttacker);
                gBattlescriptCurrInstr = BattleScript_MoveUsedMustRecharge;
                gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_FLINCH: // flinch
            if (gBattleMons[gBattlerAttacker].status2 & STATUS2_FLINCHED)
            {
                gProtectStructs[gBattlerAttacker].flinchImmobility = 1;
                CancelMultiTurnMoves(gBattlerAttacker);
                gBattlescriptCurrInstr = BattleScript_MoveUsedFlinched;
                gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_DISABLED: // disabled move
            if (gDisableStructs[gBattlerAttacker].disabledMove == gCurrentMove && gDisableStructs[gBattlerAttacker].disabledMove != 0)
            {
                gProtectStructs[gBattlerAttacker].usedDisabledMove = 1;
                gBattleScripting.battler = gBattlerAttacker;
                CancelMultiTurnMoves(gBattlerAttacker);
                gBattlescriptCurrInstr = BattleScript_MoveUsedIsDisabled;
                gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_HEAL_BLOCKED:
            if (gStatuses3[gBattlerAttacker] & STATUS3_HEAL_BLOCK && IsHealBlockPreventingMove(gBattlerAttacker, gCurrentMove))
            {
                gProtectStructs[gBattlerAttacker].usedHealBlockedMove = 1;
                gBattleScripting.battler = gBattlerAttacker;
                CancelMultiTurnMoves(gBattlerAttacker);
                gBattlescriptCurrInstr = BattleScript_MoveUsedHealBlockPrevents;
                gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_GRAVITY:
            if (gFieldStatuses & STATUS_FIELD_GRAVITY && IsGravityPreventingMove(gCurrentMove))
            {
                gProtectStructs[gBattlerAttacker].usedGravityPreventedMove = 1;
                gBattleScripting.battler = gBattlerAttacker;
                CancelMultiTurnMoves(gBattlerAttacker);
                gBattlescriptCurrInstr = BattleScript_MoveUsedGravityPrevents;
                gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_TAUNTED: // taunt
            if (gDisableStructs[gBattlerAttacker].tauntTimer && gBattleMoves[gCurrentMove].power == 0)
            {
                gProtectStructs[gBattlerAttacker].usedTauntedMove = 1;
                CancelMultiTurnMoves(gBattlerAttacker);
                gBattlescriptCurrInstr = BattleScript_MoveUsedIsTaunted;
                gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_IMPRISONED: // imprisoned
            if (GetImprisonedMovesCount(gBattlerAttacker, gCurrentMove))
            {
                gProtectStructs[gBattlerAttacker].usedImprisonedMove = 1;
                CancelMultiTurnMoves(gBattlerAttacker);
                gBattlescriptCurrInstr = BattleScript_MoveUsedIsImprisoned;
                gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_CONFUSED: // confusion
            if (gBattleMons[gBattlerAttacker].status2 & STATUS2_CONFUSION)
            {
                gBattleMons[gBattlerAttacker].status2 -= STATUS2_CONFUSION_TURN(1);
                if (gBattleMons[gBattlerAttacker].status2 & STATUS2_CONFUSION)
                {
                    if (Random() % ((B_CONFUSION_SELF_DMG_CHANCE >= GEN_7) ? 3 : 2) == 0) // confusion dmg
                    {
                        gBattleCommunication[MULTISTRING_CHOOSER] = 1;
                        gBattlerTarget = gBattlerAttacker;
                        gBattleMoveDamage = CalculateMoveDamage(MOVE_NONE, gBattlerAttacker, gBattlerAttacker, TYPE_MYSTERY, 40, FALSE, FALSE, TRUE);
                        gProtectStructs[gBattlerAttacker].confusionSelfDmg = 1;
                        gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                    }
                    else
                    {
                        gBattleCommunication[MULTISTRING_CHOOSER] = 0;
                        BattleScriptPushCursor();
                    }
                    gBattlescriptCurrInstr = BattleScript_MoveUsedIsConfused;
                }
                else // snapped out of confusion
                {
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_MoveUsedIsConfusedNoMore;
                }
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_PARALYSED: // paralysis
            if ((gBattleMons[gBattlerAttacker].status1 & STATUS1_PARALYSIS) && (Random() % 4) == 0)
            {
                gProtectStructs[gBattlerAttacker].prlzImmobility = 1;
                // This is removed in Emerald for some reason
                //CancelMultiTurnMoves(gBattlerAttacker);
                gBattlescriptCurrInstr = BattleScript_MoveUsedIsParalyzed;
                gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_IN_LOVE: // infatuation
            if (gBattleMons[gBattlerAttacker].status2 & STATUS2_INFATUATION)
            {
                gBattleScripting.battler = CountTrailingZeroBits((gBattleMons[gBattlerAttacker].status2 & STATUS2_INFATUATION) >> 0x10);
                if (Random() & 1)
                {
                    BattleScriptPushCursor();
                }
                else
                {
                    BattleScriptPush(BattleScript_MoveUsedIsInLoveCantAttack);
                    gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                    gProtectStructs[gBattlerAttacker].loveImmobility = 1;
                    CancelMultiTurnMoves(gBattlerAttacker);
                }
                gBattlescriptCurrInstr = BattleScript_MoveUsedIsInLove;
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_BIDE: // bide
            if (gBattleMons[gBattlerAttacker].status2 & STATUS2_BIDE)
            {
                gBattleMons[gBattlerAttacker].status2 -= STATUS2_BIDE_TURN(1);
                if (gBattleMons[gBattlerAttacker].status2 & STATUS2_BIDE)
                {
                    gBattlescriptCurrInstr = BattleScript_BideStoringEnergy;
                }
                else
                {
                    // This is removed in Emerald for some reason
                    //gBattleMons[gBattlerAttacker].status2 &= ~(STATUS2_MULTIPLETURNS);
                    if (gTakenDmg[gBattlerAttacker])
                    {
                        gCurrentMove = MOVE_BIDE;
                        *bideDmg = gTakenDmg[gBattlerAttacker] * 2;
                        gBattlerTarget = gTakenDmgByBattler[gBattlerAttacker];
                        if (gAbsentBattlerFlags & gBitTable[gBattlerTarget])
                            gBattlerTarget = GetMoveTarget(MOVE_BIDE, 1);
                        gBattlescriptCurrInstr = BattleScript_BideAttack;
                    }
                    else
                    {
                        gBattlescriptCurrInstr = BattleScript_BideNoEnergyToAttack;
                    }
                }
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_THAW: // move thawing
            if (gBattleMons[gBattlerAttacker].status1 & STATUS1_FREEZE)
            {
                if (gBattleMoves[gCurrentMove].effect == EFFECT_THAW_HIT
                    || (gBattleMoves[gCurrentMove].effect == EFFECT_BURN_UP && IS_BATTLER_OF_TYPE(gBattlerAttacker, TYPE_FIRE)))
                {
                    gBattleMons[gBattlerAttacker].status1 &= ~(STATUS1_FREEZE);
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_MoveUsedUnfroze;
                    gBattleCommunication[MULTISTRING_CHOOSER] = 1;
                }
                effect = 2;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_POWDER_MOVE:
            if (gBattleMoves[gCurrentMove].flags & FLAG_POWDER)
            {
                if ((B_POWDER_GRASS >= GEN_6 && IS_BATTLER_OF_TYPE(gBattlerTarget, TYPE_GRASS))
                    || GetBattlerAbility(gBattlerTarget) == ABILITY_OVERCOAT)
                {
                    gBattlerAbility = gBattlerTarget;
                    effect = 1;
                }
                else if (GetBattlerHoldEffect(gBattlerTarget, TRUE) == HOLD_EFFECT_SAFETY_GOOGLES)
                {
                    RecordItemEffectBattle(gBattlerTarget, HOLD_EFFECT_SAFETY_GOOGLES);
                    effect = 1;
                }

                if (effect)
                    gBattlescriptCurrInstr = BattleScript_PowderMoveNoEffect;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_POWDER_STATUS:
            if (gBattleMons[gBattlerAttacker].status2 & STATUS2_POWDER)
            {
                u32 moveType;
                GET_MOVE_TYPE(gCurrentMove, moveType);
                if (moveType == TYPE_FIRE)
                {
                    gProtectStructs[gBattlerAttacker].powderSelfDmg = 1;
                    gBattleMoveDamage = gBattleMons[gBattlerAttacker].maxHP / 4;
                    gBattlescriptCurrInstr = BattleScript_MoveUsedPowder;
                    effect = 1;
                }
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_THROAT_CHOP:
            if (gDisableStructs[gBattlerAttacker].throatChopTimer && gBattleMoves[gCurrentMove].flags & FLAG_SOUND)
            {
                gProtectStructs[gBattlerAttacker].usedThroatChopPreventedMove = 1;
                CancelMultiTurnMoves(gBattlerAttacker);
                gBattlescriptCurrInstr = BattleScript_MoveUsedIsThroatChopPrevented;
                gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_END:
            break;
        }

    } while (gBattleStruct->atkCancellerTracker != CANCELLER_END && gBattleStruct->atkCancellerTracker != CANCELLER_END2 && effect == 0);

    if (effect == 2)
    {
        gActiveBattler = gBattlerAttacker;
        BtlController_EmitSetMonData(0, REQUEST_STATUS_BATTLE, 0, 4, &gBattleMons[gActiveBattler].status1);
        MarkBattlerForControllerExec(gActiveBattler);
    }
    return effect;
}

// After Protean Activation.
u8 AtkCanceller_UnableToUseMove2(void)
{
    u8 effect = 0;

    do
    {
        switch (gBattleStruct->atkCancellerTracker)
        {
        case CANCELLER_END:
            gBattleStruct->atkCancellerTracker++;
        case CANCELLER_PSYCHIC_TERRAIN:
            if (gFieldStatuses & STATUS_FIELD_PSYCHIC_TERRAIN
                && IsBattlerGrounded(gBattlerTarget)
                && GetChosenMovePriority(gBattlerAttacker) > 0
                && GetBattlerSide(gBattlerAttacker) != GetBattlerSide(gBattlerTarget))
            {
                CancelMultiTurnMoves(gBattlerAttacker);
                gBattlescriptCurrInstr = BattleScript_MoveUsedPsychicTerrainPrevents;
                gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
                effect = 1;
            }
            gBattleStruct->atkCancellerTracker++;
            break;
        case CANCELLER_END2:
            break;
        }

    } while (gBattleStruct->atkCancellerTracker != CANCELLER_END2 && effect == 0);

    return effect;
}

bool8 HasNoMonsToSwitch(u8 battler, u8 partyIdBattlerOn1, u8 partyIdBattlerOn2)
{
    struct Pokemon *party;
    u8 id1, id2;
    s32 i;

    if (!(gBattleTypeFlags & BATTLE_TYPE_DOUBLE))
        return FALSE;

    if (BATTLE_TWO_VS_ONE_OPPONENT && GetBattlerSide(battler) == B_SIDE_OPPONENT)
    {
        id2 = GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT);
        id1 = GetBattlerAtPosition(B_POSITION_OPPONENT_RIGHT);
        party = gEnemyParty;

        if (partyIdBattlerOn1 == PARTY_SIZE)
            partyIdBattlerOn1 = gBattlerPartyIndexes[id2];
        if (partyIdBattlerOn2 == PARTY_SIZE)
            partyIdBattlerOn2 = gBattlerPartyIndexes[id1];

        for (i = 0; i < PARTY_SIZE; i++)
        {
            if (GetMonData(&party[i], MON_DATA_HP) != 0
             && GetMonData(&party[i], MON_DATA_SPECIES2) != SPECIES_NONE
             && GetMonData(&party[i], MON_DATA_SPECIES2) != SPECIES_EGG
             && i != partyIdBattlerOn1 && i != partyIdBattlerOn2
             && i != *(gBattleStruct->monToSwitchIntoId + id2) && i != id1[gBattleStruct->monToSwitchIntoId])
                break;
        }
        return (i == PARTY_SIZE);
    }
    else if (gBattleTypeFlags & BATTLE_TYPE_INGAME_PARTNER)
    {
        if (GetBattlerSide(battler) == B_SIDE_PLAYER)
            party = gPlayerParty;
        else
            party = gEnemyParty;

        id1 = ((battler & BIT_FLANK) / 2);
        for (i = id1 * 3; i < id1 * 3 + 3; i++)
        {
            if (GetMonData(&party[i], MON_DATA_HP) != 0
             && GetMonData(&party[i], MON_DATA_SPECIES2) != SPECIES_NONE
             && GetMonData(&party[i], MON_DATA_SPECIES2) != SPECIES_EGG)
                break;
        }
        return (i == id1 * 3 + 3);
    }
    else if (gBattleTypeFlags & BATTLE_TYPE_MULTI)
    {
        if (gBattleTypeFlags & BATTLE_TYPE_x800000)
        {
            if (GetBattlerSide(battler) == B_SIDE_PLAYER)
            {
                party = gPlayerParty;
                id2 = GetBattlerMultiplayerId(battler);
                id1 = GetLinkTrainerFlankId(id2);
            }
            else
            {
                party = gEnemyParty;
                if (battler == 1)
                    id1 = 0;
                else
                    id1 = 1;
            }
        }
        else
        {
            id2 = GetBattlerMultiplayerId(battler);

            if (GetBattlerSide(battler) == B_SIDE_PLAYER)
                party = gPlayerParty;
            else
                party = gEnemyParty;

            id1 = GetLinkTrainerFlankId(id2);
        }

        for (i = id1 * 3; i < id1 * 3 + 3; i++)
        {
            if (GetMonData(&party[i], MON_DATA_HP) != 0
             && GetMonData(&party[i], MON_DATA_SPECIES2) != SPECIES_NONE
             && GetMonData(&party[i], MON_DATA_SPECIES2) != SPECIES_EGG)
                break;
        }
        return (i == id1 * 3 + 3);
    }
    else if ((gBattleTypeFlags & BATTLE_TYPE_TWO_OPPONENTS) && GetBattlerSide(battler) == B_SIDE_OPPONENT)
    {
        party = gEnemyParty;

        if (battler == 1)
            id1 = 0;
        else
            id1 = 3;

        for (i = id1; i < id1 + 3; i++)
        {
            if (GetMonData(&party[i], MON_DATA_HP) != 0
             && GetMonData(&party[i], MON_DATA_SPECIES2) != SPECIES_NONE
             && GetMonData(&party[i], MON_DATA_SPECIES2) != SPECIES_EGG)
                break;
        }
        return (i == id1 + 3);
    }
    else
    {
        if (GetBattlerSide(battler) == B_SIDE_OPPONENT)
        {
            id2 = GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT);
            id1 = GetBattlerAtPosition(B_POSITION_OPPONENT_RIGHT);
            party = gEnemyParty;
        }
        else
        {
            id2 = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
            id1 = GetBattlerAtPosition(B_POSITION_PLAYER_RIGHT);
            party = gPlayerParty;
        }

        if (partyIdBattlerOn1 == PARTY_SIZE)
            partyIdBattlerOn1 = gBattlerPartyIndexes[id2];
        if (partyIdBattlerOn2 == PARTY_SIZE)
            partyIdBattlerOn2 = gBattlerPartyIndexes[id1];

        for (i = 0; i < PARTY_SIZE; i++)
        {
            if (GetMonData(&party[i], MON_DATA_HP) != 0
             && GetMonData(&party[i], MON_DATA_SPECIES2) != SPECIES_NONE
             && GetMonData(&party[i], MON_DATA_SPECIES2) != SPECIES_EGG
             && i != partyIdBattlerOn1 && i != partyIdBattlerOn2
             && i != *(gBattleStruct->monToSwitchIntoId + id2) && i != id1[gBattleStruct->monToSwitchIntoId])
                break;
        }
        return (i == PARTY_SIZE);
    }
}

u8 TryWeatherFormChange(u8 battler)
{
    u8 ret = 0;
    bool32 weatherEffect = WEATHER_HAS_EFFECT;

	if (gBattleMons[battler].species == SPECIES_CASTFORM && 
	    gBattleMons[battler].ability == ABILITY_FORECAST &&
		gBattleMons[battler].hp != 0){
		if (gBattleWeather & (WEATHER_RAIN_ANY) && gBattleMons[battler].species != SPECIES_CASTFORM_RAINY)
        {
            gBattleMons[battler].species = SPECIES_CASTFORM_RAINY;
			ret = 1;
        }else if (gBattleWeather & (WEATHER_SUN_ANY))
        {
            gBattleMons[battler].species = SPECIES_CASTFORM_SUNNY;
			ret = 1;
        }
		else if (gBattleWeather & (WEATHER_HAIL_ANY))
        {
            gBattleMons[battler].species = SPECIES_CASTFORM_SNOWY;
			ret = 1;
        }else
			ret = 0;
	}else if (gBattleMons[battler].species == SPECIES_CHERRIM && 
	          gBattleMons[battler].ability == ABILITY_FLOWER_GIFT &&
		      gBattleMons[battler].hp != 0){
			if (gBattleWeather & (WEATHER_SUN_ANY))
        {
            gBattleMons[battler].species = SPECIES_CHERRIM_SUNSHINE;
			ret = 1;
        }
	}

    return ret;
}
static const u16 sWeatherFlagsInfo[][3] =
{
    [ENUM_WEATHER_RAIN] = {WEATHER_RAIN_TEMPORARY, WEATHER_RAIN_PERMANENT, HOLD_EFFECT_DAMP_ROCK},
    [ENUM_WEATHER_SUN] = {WEATHER_SUN_TEMPORARY, WEATHER_SUN_PERMANENT, HOLD_EFFECT_HEAT_ROCK},
    [ENUM_WEATHER_SANDSTORM] = {WEATHER_SANDSTORM_TEMPORARY, WEATHER_SANDSTORM_PERMANENT, HOLD_EFFECT_SMOOTH_ROCK},
    [ENUM_WEATHER_HAIL] = {WEATHER_HAIL_TEMPORARY, WEATHER_HAIL_PERMANENT, HOLD_EFFECT_ICY_ROCK},
};

bool32 TryChangeBattleWeather(u8 battler, u32 weatherEnumId, bool32 viaAbility)
{
    if (viaAbility && B_ABILITY_WEATHER <= GEN_5
        && !(gBattleWeather & sWeatherFlagsInfo[weatherEnumId][1]))
    {
        gBattleWeather = (sWeatherFlagsInfo[weatherEnumId][0] | sWeatherFlagsInfo[weatherEnumId][1]);
        return TRUE;
    }
    else if (!(gBattleWeather & (sWeatherFlagsInfo[weatherEnumId][0] | sWeatherFlagsInfo[weatherEnumId][1])))
    {
        gBattleWeather = (sWeatherFlagsInfo[weatherEnumId][0]);
        if (GetBattlerHoldEffect(battler, TRUE) == sWeatherFlagsInfo[weatherEnumId][2])
            gWishFutureKnock.weatherDuration = 10;
        else
            gWishFutureKnock.weatherDuration = 8;

        return TRUE;
    }

    return FALSE;
}

static bool32 TryChangeBattleTerrain(u32 battler, u32 statusFlag, u8 *timer)
{
    if (!(gFieldStatuses & statusFlag))
    {
        gFieldStatuses &= ~(STATUS_FIELD_MISTY_TERRAIN | STATUS_FIELD_GRASSY_TERRAIN | EFFECT_ELECTRIC_TERRAIN | EFFECT_PSYCHIC_TERRAIN);
        gFieldStatuses |= statusFlag;

        if (GetBattlerHoldEffect(battler, TRUE) == HOLD_EFFECT_TERRAIN_EXTENDER)
            *timer = 10;
        else
            *timer = 8;

        gBattlerAttacker = gBattleScripting.battler = battler;
        return TRUE;
    }

    return FALSE;
}

static bool32 ShouldChangeFormHpBased(u32 battler)
{
    // Ability,     form >, form <=, hp divided
     static const u16 forms[][4] =
    {
        {ABILITY_ZEN_MODE, SPECIES_DARMANITAN, SPECIES_DARMANITAN_ZEN_MODE, 2},
		{ABILITY_ZEN_MODE, SPECIES_DARMANITAN_GALARIAN, SPECIES_DARMANITAN_ZEN_MODE_GALARIAN, 2},
        {ABILITY_SHIELDS_DOWN, SPECIES_MINIOR, SPECIES_MINIOR_CORE_RED, 2},
        {ABILITY_SHIELDS_DOWN, SPECIES_MINIOR_METEOR_BLUE, SPECIES_MINIOR_CORE_BLUE, 2},
        {ABILITY_SHIELDS_DOWN, SPECIES_MINIOR_METEOR_GREEN, SPECIES_MINIOR_CORE_GREEN, 2},
        {ABILITY_SHIELDS_DOWN, SPECIES_MINIOR_METEOR_INDIGO, SPECIES_MINIOR_CORE_INDIGO, 2},
        {ABILITY_SHIELDS_DOWN, SPECIES_MINIOR_METEOR_ORANGE, SPECIES_MINIOR_CORE_ORANGE, 2},
        {ABILITY_SHIELDS_DOWN, SPECIES_MINIOR_METEOR_VIOLET, SPECIES_MINIOR_CORE_VIOLET, 2},
        {ABILITY_SHIELDS_DOWN, SPECIES_MINIOR_METEOR_YELLOW, SPECIES_MINIOR_CORE_YELLOW, 2},
		{ABILITY_GULP_MISSILE, SPECIES_CRAMORANT, SPECIES_CRAMORANT_GORGING, 2},
        {ABILITY_GULP_MISSILE, SPECIES_CRAMORANT, SPECIES_CRAMORANT_GULPING, 1},
        {ABILITY_SCHOOLING, SPECIES_WISHIWASHI_SCHOOL, SPECIES_WISHIWASHI, 4},
    };
    u32 i;

    for (i = 0; i < ARRAY_COUNT(forms); i++)
    {
        if (gBattleMons[battler].ability == forms[i][0])
        {
            if (gBattleMons[battler].species == forms[i][2]
                && gBattleMons[battler].hp > gBattleMons[battler].maxHP / forms[i][3])
            {
                gBattleMons[battler].species = forms[i][1];
                return TRUE;
            }
            if (gBattleMons[battler].species == forms[i][1]
                && gBattleMons[battler].hp <= gBattleMons[battler].maxHP / forms[i][3])
            {
                gBattleMons[battler].species = forms[i][2];
                return TRUE;
            }
        }
    }
	
	if (gBattleMons[battler].species == SPECIES_CASTFORM && 
	    gBattleMons[battler].ability == ABILITY_FORECAST &&
		gBattleMons[battler].hp != 0){
		if (gBattleWeather & (WEATHER_RAIN_ANY) && gBattleMons[battler].species != SPECIES_CASTFORM_RAINY)
        {
            gBattleMons[battler].species = SPECIES_CASTFORM_RAINY;
			return TRUE;
        }else if (gBattleWeather & (WEATHER_SUN_ANY) && gBattleMons[battler].species != SPECIES_CASTFORM_SUNNY)
        {
            gBattleMons[battler].species = SPECIES_CASTFORM_SUNNY;
			return TRUE;
        }
		else if (gBattleWeather & (WEATHER_HAIL_ANY) && gBattleMons[battler].species != SPECIES_CASTFORM_SNOWY)
        {
            gBattleMons[battler].species = SPECIES_CASTFORM_SNOWY;
			return TRUE;
        }else
			return FALSE;
	}else if (gBattleMons[battler].species == SPECIES_CHERRIM && 
	          gBattleMons[battler].ability == ABILITY_FLOWER_GIFT &&
		      gBattleMons[battler].hp != 0){
			if (gBattleWeather & (WEATHER_SUN_ANY))
        {
            gBattleMons[battler].species = SPECIES_CHERRIM_SUNSHINE;
			return TRUE;
		}
	}

    return FALSE;
}

static u8 ForewarnChooseMove(u32 battler)
{
    struct Forewarn {
        u8 battlerId;
        u8 power;
        u16 moveId;
    };
    u32 i, j, bestId, count;
    struct Forewarn *data = malloc(sizeof(struct Forewarn) * MAX_BATTLERS_COUNT * MAX_MON_MOVES);

    // Put all moves
    for (count = 0, i = 0; i < MAX_BATTLERS_COUNT; i++)
    {
        if (IsBattlerAlive(i) && GetBattlerSide(i) != GetBattlerSide(battler))
        {
            for (j = 0; j < MAX_MON_MOVES; j++)
            {
                if (gBattleMons[i].moves[j] == MOVE_NONE)
                    continue;
                data[count].moveId = gBattleMons[i].moves[j];
                data[count].battlerId = i;
                switch (gBattleMoves[data[count].moveId].effect)
                {
                case EFFECT_OHKO:
                    data[count].power = 150;
                    break;
                case EFFECT_COUNTER:
                case EFFECT_MIRROR_COAT:
                case EFFECT_METAL_BURST:
                    data[count].power = 120;
                    break;
                default:
                    if (gBattleMoves[data[count].moveId].power == 1)
                        data[count].power = 80;
                    else
                        data[count].power = gBattleMoves[data[count].moveId].power;
                    break;
                }
                count++;
            }
        }
    }

    for (bestId = 0, i = 1; i < count; i++)
    {
        if (data[i].power > data[bestId].power)
            bestId = i;
        else if (data[i].power == data[bestId].power && Random() & 1)
            bestId = i;
    }

    gBattlerTarget = data[bestId].battlerId;
    PREPARE_MOVE_BUFFER(gBattleTextBuff1, data[bestId].moveId)
    RecordKnownMove(gBattlerTarget, data[bestId].moveId);

    free(data);
}

static void TryToRevertIceFace(u8 battlerId)
{
    if (gBattleMons[battlerId].species == SPECIES_EISCUE_NOICE_FACE
    && GetBattlerAbility(battlerId) == ABILITY_ICE_FACE
    && gBattleWeather & WEATHER_HAIL_ANY)
    {
        gBattleResources->flags->flags[battlerId] |= RESOURCE_FLAG_ICE_FACE;
        gBattlerTarget = battlerId;
        gBattleMons[gBattlerTarget].species = SPECIES_EISCUE;
        BattleScriptExecute(BattleScript_TargetFormChangeWithStringEnd2);
    }
}

u8 AbilityBattleEffects(u8 caseID, u8 battler, u16 ability, u8 special, u16 moveArg)
{
    u8 effect = 0;
    u32 speciesAtk, speciesDef;
    u32 pidAtk, pidDef;
    u32 moveType, move;
    u32 i, j;

    if (gBattleTypeFlags & BATTLE_TYPE_SAFARI)
        return 0;

    if (gBattlerAttacker >= gBattlersCount)
        gBattlerAttacker = battler;

    speciesAtk = gBattleMons[gBattlerAttacker].species;
    pidAtk = gBattleMons[gBattlerAttacker].personality;

    speciesDef = gBattleMons[gBattlerTarget].species;
    pidDef = gBattleMons[gBattlerTarget].personality;

    if (special)
        gLastUsedAbility = special;
    else
        gLastUsedAbility = GetBattlerAbility(battler);

    if (moveArg)
        move = moveArg;
    else
        move = gCurrentMove;

    GET_MOVE_TYPE(move, moveType);

    switch (caseID)
    {
    case ABILITYEFFECT_ON_SWITCHIN: // 0
        gBattleScripting.battler = battler;
        switch (gLastUsedAbility)
        {
        case ABILITYEFFECT_SWITCH_IN_WEATHER:
            if (!(gBattleTypeFlags & BATTLE_TYPE_RECORDED))
            {
                switch (GetCurrentWeather())
                {
                case WEATHER_RAIN:
                case WEATHER_RAIN_THUNDERSTORM:
                case WEATHER_DOWNPOUR:
                    if (!(gBattleWeather & WEATHER_RAIN_ANY))
                    {
                        gBattleWeather = (WEATHER_RAIN_TEMPORARY | WEATHER_RAIN_PERMANENT);
                        gBattleScripting.animArg1 = B_ANIM_RAIN_CONTINUES;
                        effect++;
                    }
                    break;
                case WEATHER_SANDSTORM:
                    if (!(gBattleWeather & WEATHER_SANDSTORM_ANY))
                    {
                        gBattleWeather = (WEATHER_SANDSTORM_PERMANENT | WEATHER_SANDSTORM_TEMPORARY);
                        gBattleScripting.animArg1 = B_ANIM_SANDSTORM_CONTINUES;
                        effect++;
                    }
                    break;
                case WEATHER_DROUGHT:
                    if (!(gBattleWeather & WEATHER_SUN_ANY))
                    {
                        gBattleWeather = (WEATHER_SUN_PERMANENT | WEATHER_SUN_TEMPORARY);
                        gBattleScripting.animArg1 = B_ANIM_SUN_CONTINUES;
                        effect++;
                    }
                    break;
                }
            }
            if (effect)
            {
                gBattleCommunication[MULTISTRING_CHOOSER] = GetCurrentWeather();
                BattleScriptPushCursorAndCallback(BattleScript_OverworldWeatherStarts);
            }
            break;
        case ABILITY_IMPOSTER:
            if (IsBattlerAlive(BATTLE_OPPOSITE(battler))
                && !(gBattleMons[BATTLE_OPPOSITE(battler)].status2 & (STATUS2_TRANSFORMED | STATUS2_SUBSTITUTE))
                && !(gBattleMons[battler].status2 & STATUS2_TRANSFORMED)
                && !(gBattleStruct->illusion[BATTLE_OPPOSITE(battler)].on)
                && !(gStatuses3[BATTLE_OPPOSITE(battler)] & STATUS3_SEMI_INVULNERABLE))
            {
                gBattlerTarget = BATTLE_OPPOSITE(battler);
                BattleScriptPushCursorAndCallback(BattleScript_ImposterActivates);
                effect++;
            }
            break;
        case ABILITY_MOLD_BREAKER:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
                gBattleCommunication[MULTISTRING_CHOOSER] = MULTI_SWITCHIN_MOLDBREAKER;
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_SwitchInAbilityMsg);
                effect++;
            }
            break;
        case ABILITY_TERAVOLT:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
                gBattleCommunication[MULTISTRING_CHOOSER] = MULTI_SWITCHIN_TERAVOLT;
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_SwitchInAbilityMsg);
                effect++;
            }
            break;
        case ABILITY_TURBOBLAZE:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
                gBattleCommunication[MULTISTRING_CHOOSER] = MULTI_SWITCHIN_TURBOBLAZE;
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_SwitchInAbilityMsg);
                effect++;
            }
            break;
        case ABILITY_SLOW_START:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
                gDisableStructs[battler].slowStartTimer = 5;
                gBattleCommunication[MULTISTRING_CHOOSER] = MULTI_SWITCHIN_SLOWSTART;
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_SwitchInAbilityMsg);
                effect++;
            }
            break;
        case ABILITY_UNNERVE:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
                gBattleCommunication[MULTISTRING_CHOOSER] = MULTI_SWITCHIN_UNNERVE;
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_SwitchInAbilityMsg);
                effect++;
            }
            break;
        case ABILITY_CURIOUS_MEDICINE:
            if (!gSpecialStatuses[battler].switchInAbilityDone && TryResetAllBattlersStatChanges())
            {
                u32 i;
                gEffectBattler = BATTLE_PARTNER(battler);
                gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_SWITCHIN_CURIOUS_MEDICINE;
                gSpecialStatuses[battler].switchInAbilityDone = TRUE;
                BattleScriptPushCursorAndCallback(BattleScript_SwitchInAbilityMsg);
                effect++;
            }
            break;
        case ABILITY_PASTEL_VEIL:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
                gBattlerTarget = battler;
                gBattleCommunication[MULTISTRING_CHOOSER] = B_MSG_SWITCHIN_PASTEL_VEIL;
                BattleScriptPushCursorAndCallback(BattleScript_PastelVeilActivates);
                effect++;
                gSpecialStatuses[battler].switchInAbilityDone = 1;
            }
            break;
        case ABILITY_ANTICIPATION:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
                u32 side = GetBattlerSide(battler);

                for (i = 0; i < MAX_BATTLERS_COUNT; i++)
                {
                    if (IsBattlerAlive(i) && side != GetBattlerSide(i))
                    {
                        for (j = 0; j < MAX_MON_MOVES; j++)
                        {
                            move = gBattleMons[i].moves[j];
                            GET_MOVE_TYPE(move, moveType);
                            if (CalcTypeEffectivenessMultiplier(move, moveType, i, battler, FALSE) >= UQ_4_12(2.0))
                            {
                                effect++;
                                break;
                            }
                        }
                    }
                }

                if (effect)
                {
                    gBattleCommunication[MULTISTRING_CHOOSER] = MULTI_SWITCHIN_ANTICIPATION;
                    gSpecialStatuses[battler].switchInAbilityDone = 1;
                    BattleScriptPushCursorAndCallback(BattleScript_SwitchInAbilityMsg);
                }
            }
            break;
        case ABILITY_FRISK:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_FriskActivates); // Try activate
                effect++;
            }
            return effect; // Note: It returns effect as to not record the ability if Frisk does not activate.
		case ABILITY_AIR_CURRENT:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_EffectAirCurrent); // Try activate
                effect++;
            }
			break;
		case ABILITY_WATER_SPILL:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
				gFieldTimers.waterSportTimer = 5;
                gFieldStatuses |= STATUS_FIELD_WATERSPORT;
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_EffectWaterSpill); // Try activate
                effect++;
            }
			break;
		case ABILITY_AURORA_BODY:
            if (!gSpecialStatuses[battler].switchInAbilityDone && (gBattleWeather & WEATHER_HAIL_ANY))
            {
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_EffectAuroraBody); // Try activate
                effect++;
            }
			break;
		case ABILITY_SCREEN_SETTER:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_EffectScreenSetter); // Try activate
                effect++;
            }
			break;
		case ABILITY_DREAM_WORLD:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
				//Self
				gStatuses3[battler] = STATUS3_YAWN_TURN(2);
				//gStatuses3[battler] |= STATUS3_YAWN;
				//Enemy
				gStatuses3[BATTLE_OPPOSITE(battler)] = STATUS3_YAWN_TURN(2);
				//gStatuses3[BATTLE_OPPOSITE(battler)] |= STATUS3_YAWN;
				
				if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE){
					//Ally
					gStatuses3[BATTLE_PARTNER(battler)] = STATUS3_YAWN_TURN(2);
					//Enemy 2
					gStatuses3[BATTLE_OPPOSITE(BATTLE_PARTNER(battler))] = STATUS3_YAWN_TURN(2);
				}
				
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_EffectDreamWorld); // Try activate
                effect++;
            }
			break;
		case ABILITY_NEUTRALIZING_GAS:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
				u8 i;
				for (i = 0; i < gBattlersCount; i++)
				{
					if(GetBattlerAbility(i) != ABILITY_MULTITYPE &&
					   GetBattlerAbility(i) != ABILITY_ZEN_MODE &&
				       GetBattlerAbility(i) != ABILITY_STANCE_CHANGE &&
				       GetBattlerAbility(i) != ABILITY_POWER_CONSTRUCT &&
				       GetBattlerAbility(i) != ABILITY_SCHOOLING &&
				       GetBattlerAbility(i) != ABILITY_RKS_SYSTEM &&
				       GetBattlerAbility(i) != ABILITY_SHIELDS_DOWN &&
				       GetBattlerAbility(i) != ABILITY_BATTLE_BOND &&
				       GetBattlerAbility(i) != ABILITY_COMATOSE &&
				       GetBattlerAbility(i) != ABILITY_DISGUISE &&
				       GetBattlerAbility(i) != ABILITY_GULP_MISSILE &&
				       GetBattlerAbility(i) != ABILITY_ICE_FACE)
						gStatuses3[i] = STATUS3_GASTRO_ACID;
				}
				
				gBattleCommunication[MULTISTRING_CHOOSER] = MULTI_SWITCHIN_NEUTRALIZING_GAS;
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_SwitchInAbilityMsg);
                effect++;
            }
			break;
		case ABILITY_TRICKSTER:
            if (!gSpecialStatuses[battler].switchInAbilityDone && !(gFieldStatuses & STATUS_FIELD_TRICK_ROOM))
            {
				if (GetBattlerHoldEffect(battler, TRUE) == HOLD_EFFECT_TERRAIN_EXTENDER)
					gFieldTimers.trickRoomTimer = 8;
				else
					gFieldTimers.trickRoomTimer = 5;
				
                gFieldStatuses |= STATUS_FIELD_TRICK_ROOM;
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_EffectTrickster); // Try activate
                effect++;
            }
            else if(!gSpecialStatuses[battler].switchInAbilityDone && (gFieldStatuses & STATUS_FIELD_TRICK_ROOM)){
                gFieldTimers.trickRoomTimer = 0;
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptExecute(BattleScript_TrickRoomEnds);
                //BattleScriptPushCursorAndCallback(BattleScript_EffectTrickster); // Try activate
                effect++;
            }
			break;
		case ABILITY_OPPOSITE_DAY:
            if (!gSpecialStatuses[battler].switchInAbilityDone && !(gFieldStatuses & STATUS_FIELD_INVERSE_ROOM))
            {
				if (GetBattlerHoldEffect(battler, TRUE) == HOLD_EFFECT_TERRAIN_EXTENDER)
					gFieldTimers.inverseRoomTimer = 8;
				else
					gFieldTimers.inverseRoomTimer = 5;
				
                gFieldStatuses |= STATUS_FIELD_INVERSE_ROOM;
				
				//gLastUsedMove = MOVE_TRICK_ROOM;
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_EffectOppositeDay); // Try activate
                effect++;
            }
            else if(!gSpecialStatuses[battler].switchInAbilityDone && (gFieldStatuses & STATUS_FIELD_INVERSE_ROOM)){
                gFieldTimers.inverseRoomTimer = 0;
                gFieldStatuses &= ~(STATUS_FIELD_INVERSE_ROOM);
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptExecute(BattleScript_TrickRoomEnds);
                //BattleScriptPushCursorAndCallback(BattleScript_EffectOppositeDay); // Try activate
                effect++;
            }
			break;
		case ABILITY_GRAVITATION:
            if (!gSpecialStatuses[battler].switchInAbilityDone && !(gFieldStatuses & STATUS_FIELD_GRAVITY))
            {
                gSpecialStatuses[battler].switchInAbilityDone = 1;
				gFieldTimers.gravityTimer = 5;
				
                gFieldStatuses |= STATUS_FIELD_GRAVITY;
                BattleScriptPushCursorAndCallback(BattleScript_EffectGravitation); // Try activate
                effect++;
            }
			break;
        case ABILITY_FOREWARN:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
                ForewarnChooseMove(battler);
                gBattleCommunication[MULTISTRING_CHOOSER] = MULTI_SWITCHIN_FOREWARN;
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_SwitchInAbilityMsg);
                effect++;
            }
            break;
        case ABILITY_DOWNLOAD:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
                u32 statId, opposingBattler;
                u32 opposingDef = 0, opposingSpDef = 0;

                opposingBattler = BATTLE_OPPOSITE(battler);
                for (i = 0; i < 2; opposingBattler ^= BIT_SIDE, i++)
                {
                    if (IsBattlerAlive(opposingBattler))
                    {
                        opposingDef += gBattleMons[opposingBattler].defense
                                    * gStatStageRatios[gBattleMons[opposingBattler].statStages[STAT_DEF]][0]
                                    / gStatStageRatios[gBattleMons[opposingBattler].statStages[STAT_DEF]][1];
                        opposingSpDef += gBattleMons[opposingBattler].spDefense
                                      * gStatStageRatios[gBattleMons[opposingBattler].statStages[STAT_SPDEF]][0]
                                      / gStatStageRatios[gBattleMons[opposingBattler].statStages[STAT_SPDEF]][1];
                    }
                }

                if (opposingDef < opposingSpDef)
                    statId = STAT_ATK;
                else
                    statId = STAT_SPATK;

                gSpecialStatuses[battler].switchInAbilityDone = 1;

                if (gBattleMons[battler].statStages[statId] != 12)
                {
					gBattlerAttacker = battler;
                    gBattleMons[battler].statStages[statId]++;
                    SET_STATCHANGER(statId, 1, FALSE);
                    PREPARE_STAT_BUFFER(gBattleTextBuff1, statId);
                    BattleScriptPushCursorAndCallback(BattleScript_AttackerAbilityStatRaiseEnd3);
                    effect++;
                }
            }
            break;
        case ABILITY_PRESSURE:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
                gBattleCommunication[MULTISTRING_CHOOSER] = MULTI_SWITCHIN_PRESSURE;
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_SwitchInAbilityMsg);
                effect++;
            }
            break;
        case ABILITY_DARK_AURA:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
                gBattleCommunication[MULTISTRING_CHOOSER] = MULTI_SWITCHIN_DARKAURA;
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_SwitchInAbilityMsg);
                effect++;
            }
            break;
        case ABILITY_FAIRY_AURA:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
                gBattleCommunication[MULTISTRING_CHOOSER] = MULTI_SWITCHIN_FAIRYAURA;
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_SwitchInAbilityMsg);
                effect++;
            }
            break;
        case ABILITY_AURA_BREAK:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
                gBattleCommunication[MULTISTRING_CHOOSER] = MULTI_SWITCHIN_AURABREAK;
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_SwitchInAbilityMsg);
                effect++;
            }
            break;
        case ABILITY_COMATOSE:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
                gBattleCommunication[MULTISTRING_CHOOSER] = MULTI_SWITCHIN_COMATOSE;
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_SwitchInAbilityMsg);
                effect++;
            }
            break;
        case ABILITY_SCREEN_CLEANER:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
                gBattleCommunication[MULTISTRING_CHOOSER] = MULTI_SWITCHIN_SCREENCLEANER;
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_SwitchInAbilityMsg);
                effect++;
            }
            break;
        case ABILITY_DRIZZLE:
            if (TryChangeBattleWeather(battler, ENUM_WEATHER_RAIN, TRUE))
            {
                BattleScriptPushCursorAndCallback(BattleScript_DrizzleActivates);
                effect++;
            }
            break;
        case ABILITY_SAND_STREAM:
            if (TryChangeBattleWeather(battler, ENUM_WEATHER_SANDSTORM, TRUE))
            {
                BattleScriptPushCursorAndCallback(BattleScript_SandstreamActivates);
                effect++;
            }
            break;
        case ABILITY_DROUGHT:
            if (TryChangeBattleWeather(battler, ENUM_WEATHER_SUN, TRUE))
            {
                BattleScriptPushCursorAndCallback(BattleScript_DroughtActivates);
                effect++;
            }
            break;
        case ABILITY_SNOW_WARNING:
            if (TryChangeBattleWeather(battler, ENUM_WEATHER_HAIL, TRUE))
            {
                BattleScriptPushCursorAndCallback(BattleScript_SnowWarningActivates);
                effect++;
            }
            break;
        case ABILITY_ELECTRIC_SURGE:
            if (TryChangeBattleTerrain(battler, STATUS_FIELD_ELECTRIC_TERRAIN, &gFieldTimers.electricTerrainTimer))
            {
                BattleScriptPushCursorAndCallback(BattleScript_ElectricSurgeActivates);
                effect++;
            }
            break;
        case ABILITY_GRASSY_SURGE:
            if (TryChangeBattleTerrain(battler, STATUS_FIELD_GRASSY_TERRAIN, &gFieldTimers.grassyTerrainTimer))
            {
                BattleScriptPushCursorAndCallback(BattleScript_GrassySurgeActivates);
                effect++;
            }
            break;
        case ABILITY_MISTY_SURGE:
            if (TryChangeBattleTerrain(battler, STATUS_FIELD_MISTY_TERRAIN, &gFieldTimers.mistyTerrainTimer))
            {
                BattleScriptPushCursorAndCallback(BattleScript_MistySurgeActivates);
                effect++;
            }
            break;
        case ABILITY_PSYCHIC_SURGE:
            if (TryChangeBattleTerrain(battler, STATUS_FIELD_PSYCHIC_TERRAIN, &gFieldTimers.psychicTerrainTimer))
            {
                BattleScriptPushCursorAndCallback(BattleScript_PsychicSurgeActivates);
                effect++;
            }
            break;
        case ABILITY_INTIMIDATE:
            if (!(gSpecialStatuses[battler].intimidatedMon))
            {
                gBattleResources->flags->flags[battler] |= RESOURCE_FLAG_INTIMIDATED;
                gSpecialStatuses[battler].intimidatedMon = 1;
            }
            break;
		case ABILITY_FORECAST:
			if(gBattleMons[battler].item == ITEM_DAMP_ROCK && gBattleMons[battler].species == SPECIES_CASTFORM && !(gBattleWeather & WEATHER_RAIN_ANY)){
				if (TryChangeBattleWeather(battler, ENUM_WEATHER_RAIN, TRUE))
				{
					BattleScriptPushCursorAndCallback(BattleScript_DrizzleActivates);
					gBattleMons[battler].species = SPECIES_CASTFORM_RAINY;
					BattleScriptPushCursorAndCallback(BattleScript_AttackerFormChangeEnd3);
					effect++;
				}
			}
			else if(gBattleMons[battler].item == ITEM_ICY_ROCK && gBattleMons[battler].species == SPECIES_CASTFORM && !(gBattleWeather & WEATHER_HAIL_ANY)){
				if (TryChangeBattleWeather(battler, ENUM_WEATHER_HAIL, TRUE))
				{
					BattleScriptPushCursorAndCallback(BattleScript_SnowWarningActivates);
					gBattleMons[battler].species = SPECIES_CASTFORM_SNOWY;
					BattleScriptPushCursorAndCallback(BattleScript_AttackerFormChangeEnd3);
					effect++;
				}
			}
			else if(gBattleMons[battler].item == ITEM_HEAT_ROCK && gBattleMons[battler].species == SPECIES_CASTFORM && !(gBattleWeather & WEATHER_SUN_ANY)){
				if (TryChangeBattleWeather(battler, ENUM_WEATHER_SUN, TRUE))
				{
					BattleScriptPushCursorAndCallback(BattleScript_DroughtActivates);
					gBattleMons[battler].species = SPECIES_CASTFORM_SUNNY;
					BattleScriptPushCursorAndCallback(BattleScript_AttackerFormChangeEnd3);
					effect++;
				}
			}
			else if (ShouldChangeFormHpBased(battler))
            {
                BattleScriptPushCursorAndCallback(BattleScript_AttackerFormChangeEnd3);
                effect++;
            }
            break;
        case ABILITY_FLOWER_GIFT:
			if(gBattleMons[battler].item == ITEM_HEAT_ROCK && gBattleMons[battler].species == SPECIES_CHERRIM){
				if (TryChangeBattleWeather(battler, ENUM_WEATHER_SUN, TRUE))
				{
					BattleScriptPushCursorAndCallback(BattleScript_DroughtActivates);
					gBattleMons[battler].species = SPECIES_CHERRIM_SUNSHINE;
					BattleScriptPushCursorAndCallback(BattleScript_AttackerFormChangeEnd3);
					effect++;
				}
			}
			else if(gBattleMons[battler].item == ITEM_HEAT_ROCK && gBattleMons[battler].species != SPECIES_CHERRIM){
				if (TryChangeBattleWeather(battler, ENUM_WEATHER_SUN, TRUE))
				{
					BattleScriptPushCursorAndCallback(BattleScript_DroughtActivates);
					effect++;
				}
			}
            break;
        case ABILITY_TRACE:
            if (!(gSpecialStatuses[battler].traced))
            {
                gBattleResources->flags->flags[battler] |= RESOURCE_FLAG_TRACED;
                gSpecialStatuses[battler].traced = 1;
            }
            break;
        case ABILITY_CLOUD_NINE:
        case ABILITY_AIR_LOCK:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
                gSpecialStatuses[battler].switchInAbilityDone = 1;
                BattleScriptPushCursorAndCallback(BattleScript_AnnounceAirLockCloudNine);
                effect++;
            }
            break;
        case ABILITY_SCHOOLING:
            if (gBattleMons[battler].level < 20)
                break;
        case ABILITY_SHIELDS_DOWN:
            if (ShouldChangeFormHpBased(battler))
            {
                BattleScriptPushCursorAndCallback(BattleScript_AttackerFormChangeEnd3);
                effect++;
            }
            break;
		case ABILITY_POSSESSED:
            if (!gSpecialStatuses[battler].switchInAbilityDone)
            {
				gBattlerAttacker = battler;
                gSpecialStatuses[battler].switchInAbilityDone = TRUE;
				gBattleMons[battler].type3 = TYPE_GHOST;
				PREPARE_TYPE_BUFFER(gBattleTextBuff1, gBattleMons[battler].type3);
				BattleScriptPushCursorAndCallback(BattleScript_BattlerGotTheType);
				effect++;
            }
            break;
		case ABILITY_BATTERY:
            if (!gSpecialStatuses[battler].switchInAbilityDone  &&
				!(gStatuses3[battler] & STATUS3_CHARGED_UP))
            {
				gBattlerAttacker = battler;
				gSpecialStatuses[battler].switchInAbilityDone = TRUE;
                SET_STATCHANGER(STAT_SPDEF, 1, FALSE);
				gStatuses3[battler] = STATUS3_CHARGED_UP;
                BattleScriptPushCursorAndCallback(BattleScript_BattlerAbilityStatRaiseOnSwitchIn);
                effect++;
            }
            break;
		case ABILITY_PICKUP:
            if (!gSpecialStatuses[battler].switchInAbilityDone &&
			   ((gSideStatuses[GetBattlerSide(gActiveBattler)] & SIDE_STATUS_STEALTH_ROCK)   ||
                (gSideStatuses[GetBattlerSide(gActiveBattler)] & SIDE_STATUS_TOXIC_SPIKES)   ||
                (gSideStatuses[GetBattlerSide(gActiveBattler)] & SIDE_STATUS_SPIKES_DAMAGED) ||
                (gSideStatuses[GetBattlerSide(gActiveBattler)] & SIDE_STATUS_STICKY_WEB)))
            {
				gBattlerAttacker = battler;
				gSpecialStatuses[battler].switchInAbilityDone = TRUE;
                gSideStatuses[GetBattlerSide(gActiveBattler)] &= ~(SIDE_STATUS_STEALTH_ROCK | SIDE_STATUS_TOXIC_SPIKES | SIDE_STATUS_SPIKES_DAMAGED | SIDE_STATUS_STICKY_WEB);
                BattleScriptPushCursorAndCallback(BattleScript_PickUpActivate);
                effect++;
            }
            break;
		/*case ABILITY_RUN_AWAY:
            if (!gSpecialStatuses[battler].switchInAbilityDone && 
				 gBattleMons[battler].statStages[STAT_SPEED] != 12)
            {
				gBattlerAttacker = battler;
                gBattleMons[battler].statStages[STAT_SPEED]++;
                gSpecialStatuses[battler].switchInAbilityDone = TRUE;
                SET_STATCHANGER(STAT_SPEED, 1, FALSE);
                BattleScriptPushCursorAndCallback(BattleScript_BattlerAbilityStatRaiseOnSwitchIn);
                effect++;
            }
			break;*/
		case ABILITY_ILLUMINATE:
            if (!gSpecialStatuses[battler].switchInAbilityDone && 
				 gBattleMons[battler].statStages[STAT_ACC] != 12)
            {
				gBattlerAttacker = battler;
                gBattleMons[battler].statStages[STAT_ACC]++;
                gSpecialStatuses[battler].switchInAbilityDone = TRUE;
                SET_STATCHANGER(STAT_ACC, 1, FALSE);
                BattleScriptPushCursorAndCallback(BattleScript_BattlerAbilityStatRaiseOnSwitchIn);
                effect++;
            }
			break;
		case ABILITY_ROOTED:
            if (!gSpecialStatuses[battler].switchInAbilityDone &&
				!(gStatuses3[battler] & STATUS3_ROOTED))
            {
				gBattlerAttacker = battler;
                gSpecialStatuses[battler].switchInAbilityDone = 1;
				gStatuses3[battler] = STATUS3_ROOTED;
                BattleScriptPushCursorAndCallback(BattleScript_EffectRooted); // Try activate
                effect++;
            }
			break;
		case ABILITY_COLOR_CHANGE:
            if (!gSpecialStatuses[battler].switchInAbilityDone && 
			    gBattleMons[BATTLE_OPPOSITE(battler)].type1 != gBattleMons[battler].type1 &&
				gBattleMons[BATTLE_OPPOSITE(battler)].type1 != gBattleMons[battler].type2)
            {
				gBattlerAttacker = battler;
                gSpecialStatuses[battler].switchInAbilityDone = TRUE;
				gBattleMons[battler].type3 = gBattleMons[BATTLE_OPPOSITE(battler)].type1;
				PREPARE_TYPE_BUFFER(gBattleTextBuff1, gBattleMons[battler].type3);
				BattleScriptPushCursorAndCallback(BattleScript_BattlerGotTheType);
				effect++;
            }
			break;
		case ABILITY_MAGNETIC_BODY:
            if (!gSpecialStatuses[battler].switchInAbilityDone &&
				!(gStatuses3[battler] & STATUS3_MAGNET_RISE))
            {
				gBattlerAttacker = battler;
				gSpecialStatuses[battler].switchInAbilityDone = 1;
				gDisableStructs[battler].magnetRiseTimer = 5;
				gStatuses3[battler] = STATUS3_MAGNET_RISE;
                BattleScriptPushCursorAndCallback(BattleScript_EffectMagneticBody); // Try activate
                effect++;
            }
			break;
		case ABILITY_COILED_UP:
            if (!gSpecialStatuses[battler].switchInAbilityDone &&
				!(gStatuses4[battler] & STATUS4_COILED_UP))
            {
				gBattlerAttacker = battler;
				gSpecialStatuses[battler].switchInAbilityDone = 1;
				gStatuses4[battler] |= STATUS4_COILED_UP;
                BattleScriptPushCursorAndCallback(BattleScript_EffectCoiledUp); // Try activate
                effect++;
            }
			break;
        case ABILITY_SUPREME_OVERLORD:
            if (!gSpecialStatuses[battler].switchInAbilityDone && CountUsablePartyMons(battler) < PARTY_SIZE)
            {
                gSpecialStatuses[battler].switchInAbilityDone = TRUE;
                BattleScriptPushCursorAndCallback(BattleScript_SupremeOverlordActivates);
                effect++;
            }
            break;
        }

		if(FlagGet(FLAG_TOTEM_BATTLE) 
			&& GetBattlerSide(battler) != B_SIDE_PLAYER 
		    && VarGet(VAR_NUM_WILD_POKEMON_STAT_BOOST) != 0
			&& gBattleResults.battleTurnCounter == 0
			&& !(gBattleTypeFlags & BATTLE_TYPE_TRAINER)){
			u8 j;
			u8 numrise = 1;  
			//u8 numrise = VarGet(VAR_NUM_WILD_POKEMON_STAT_BOOST);
			FlagSet(FLAG_SMART_AI);
			
			gBattlerAttacker = battler;
			gSpecialStatuses[battler].switchInAbilityDone = 1;
			for(j = 0; j < NUM_STATS; j++)
				gBattleMons[battler].statStages[j] = gBattleMons[battler].statStages[j] + numrise;
			
			gBattleMons[battler].statStages[STAT_ACC] = gBattleMons[battler].statStages[STAT_ACC] + 2; 
			
			SET_STATCHANGER(STAT_SPATK, 1, FALSE);
            BattleScriptPushCursorAndCallback(BattleScript_WildTotemBoostActivated); // Try activate
            effect++;

            //Removing these flags are handled in HandleEndTurn_FinishBattle
		}
		
        break;
    case ABILITYEFFECT_ENDTURN: // 1
        if (gBattleMons[battler].hp != 0)
        {
            gBattlerAttacker = battler;
            switch (gLastUsedAbility)
            {
            case ABILITY_HARVEST:
                if (((WEATHER_HAS_EFFECT && gBattleWeather & WEATHER_SUN_ANY) || Random() % 2 == 0)
                 && gBattleMons[battler].item == ITEM_NONE
                 && gBattleStruct->changedItems[battler] == ITEM_NONE
                 && ItemId_GetPocket(gBattleStruct->usedHeldItems[battler]) == POCKET_BERRIES)
                {
                    gLastUsedItem = gBattleStruct->changedItems[battler] = gBattleStruct->usedHeldItems[battler];
                    gBattleStruct->usedHeldItems[battler] = ITEM_NONE;
                    BattleScriptPushCursorAndCallback(BattleScript_HarvestActivates);
                    effect++;
                }
                break;
            case ABILITY_DRY_SKIN:
                if (gBattleWeather & WEATHER_SUN_ANY)
                    goto SOLAR_POWER_HP_DROP;
            // Dry Skin works similarly to Rain Dish in Rain
            case ABILITY_RAIN_DISH:
                if (WEATHER_HAS_EFFECT
                 && (gBattleWeather & WEATHER_RAIN_ANY)
                 && !BATTLER_MAX_HP(battler)
                 && !(gStatuses3[battler] & STATUS3_HEAL_BLOCK))
                {
                    BattleScriptPushCursorAndCallback(BattleScript_RainDishActivates);
                    gBattleMoveDamage = gBattleMons[battler].maxHP / (gLastUsedAbility == ABILITY_RAIN_DISH ? 16 : 8);
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = 1;
                    gBattleMoveDamage *= -1;
                    effect++;
                }
                break;
            case ABILITY_HYDRATION:
                if (WEATHER_HAS_EFFECT
                 && (gBattleWeather & WEATHER_RAIN_ANY)
                 && gBattleMons[battler].status1 & STATUS1_ANY)
                {
                    goto ABILITY_HEAL_MON_STATUS;
                }
                break;
            case ABILITY_SHED_SKIN:
                if ((gBattleMons[battler].status1 & STATUS1_ANY) && (Random() % 3) == 0)
                {
                ABILITY_HEAL_MON_STATUS:
                    if (gBattleMons[battler].status1 & (STATUS1_POISON | STATUS1_TOXIC_POISON))
                        StringCopy(gBattleTextBuff1, gStatusConditionString_PoisonJpn);
                    if (gBattleMons[battler].status1 & STATUS1_SLEEP)
                        StringCopy(gBattleTextBuff1, gStatusConditionString_SleepJpn);
                    if (gBattleMons[battler].status1 & STATUS1_PARALYSIS)
                        StringCopy(gBattleTextBuff1, gStatusConditionString_ParalysisJpn);
                    if (gBattleMons[battler].status1 & STATUS1_BURN)
                        StringCopy(gBattleTextBuff1, gStatusConditionString_BurnJpn);
                    if (gBattleMons[battler].status1 & STATUS1_FREEZE)
                        StringCopy(gBattleTextBuff1, gStatusConditionString_IceJpn);

                    gBattleMons[battler].status1 = 0;
                    gBattleMons[battler].status2 &= ~(STATUS2_NIGHTMARE);
                    gBattleScripting.battler = gActiveBattler = battler;
                    BattleScriptPushCursorAndCallback(BattleScript_ShedSkinActivates);
                    BtlController_EmitSetMonData(0, REQUEST_STATUS_BATTLE, 0, 4, &gBattleMons[battler].status1);
                    MarkBattlerForControllerExec(gActiveBattler);
                    effect++;
                }
                break;
            case ABILITY_SPEED_BOOST:
                if (gBattleMons[battler].statStages[STAT_SPEED] < 0xC && gDisableStructs[battler].isFirstTurn != 2)
                {
                    gBattleMons[battler].statStages[STAT_SPEED]++;
                    gBattleScripting.animArg1 = 0x11;
                    gBattleScripting.animArg2 = 0;
                    BattleScriptPushCursorAndCallback(BattleScript_SpeedBoostActivates);
                    gBattleScripting.battler = battler;
                    effect++;
                }
                break;
			case ABILITY_AUTO_HEAL:
                if (!BATTLER_MAX_HP(battler) && !(gStatuses3[gActiveBattler] & STATUS3_HEAL_BLOCK) && gDisableStructs[battler].isFirstTurn != 2)
				{
                    gBattleMoveDamage = gBattleMons[gActiveBattler].maxHP / 8;
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = 1;
                    gBattleMoveDamage *= -1;
                    BattleScriptExecute(BattleScript_AutoHealActivates);
                    effect++;
                }
                break;
            case ABILITY_MOODY:
                if (gDisableStructs[battler].isFirstTurn != 2)
                {
                    u32 validToRaise = 0, validToLower = 0;
                    u32 statsNum = (B_MOODY_ACC_EVASION != GEN_8) ? NUM_BATTLE_STATS : NUM_STATS;

                    for (i = STAT_ATK; i < statsNum; i++)
                    {
                        if (gBattleMons[battler].statStages[i] != 0)
                            validToLower |= gBitTable[i];
                        if (gBattleMons[battler].statStages[i] != 12)
                            validToRaise |= gBitTable[i];
                    }

                    if (validToLower != 0 || validToRaise != 0) // Can lower one stat, or can raise one stat
                    {
                        gBattleScripting.statChanger = gBattleScripting.savedStatChanger = 0; // for raising and lowering stat respectively
                        if (validToRaise != 0) // Find stat to raise
                        {
                            do
                            {
                                i = (Random() % statsNum) + STAT_ATK;
                            } while (!(validToRaise & gBitTable[i]));
                            SET_STATCHANGER(i, 2, FALSE);
                            validToLower &= ~(gBitTable[i]); // Can't lower the same stat as raising.
                        }
                        if (validToLower != 0) // Find stat to lower
                        {
                            do
                            {
                                i = (Random() % statsNum) + STAT_ATK;
                            } while (!(validToLower & gBitTable[i]));
                            SET_STATCHANGER2(gBattleScripting.savedStatChanger, i, 1, TRUE);
                        }
                        BattleScriptPushCursorAndCallback(BattleScript_MoodyActivates);
                        effect++;
                    }
                }
                break;
            case ABILITY_TRUANT:
                if(gBattleMons[battler].species == SPECIES_SLAKING || gBattleMons[battler].species == SPECIES_SLAKOTH){
                    if(gLastMoves[gBattlerAttacker]!= MOVE_SLACK_OFF &&
                    !(gStatuses3[gBattlerAttacker] & STATUS3_GASTRO_ACID))
                        gDisableStructs[gBattlerAttacker].truantCounter ^= 1;
                    else
                        gDisableStructs[gBattlerAttacker].truantCounter = 0;
                }
                else if(gStatuses3[gBattlerAttacker] & STATUS3_GASTRO_ACID)
                    gDisableStructs[gBattlerAttacker].truantCounter = 0;
                else
                    gDisableStructs[gBattlerAttacker].truantCounter ^= 1;
            case ABILITY_BAD_DREAMS:
                if (gBattleMons[battler].status1 & STATUS1_SLEEP
                    || gBattleMons[BATTLE_OPPOSITE(battler)].status1 & STATUS1_SLEEP
                    || GetBattlerAbility(battler) == ABILITY_COMATOSE
                    || GetBattlerAbility(BATTLE_OPPOSITE(battler)) == ABILITY_COMATOSE)
                {
                    BattleScriptPushCursorAndCallback(BattleScript_BadDreamsActivates);
                    effect++;
                }
                break;
            SOLAR_POWER_HP_DROP:
            /*/case ABILITY_SOLAR_POWER:
                if (WEATHER_HAS_EFFECT && gBattleWeather & WEATHER_SUN_ANY)
                {
                    BattleScriptPushCursorAndCallback(BattleScript_SolarPowerActivates);
                    gBattleMoveDamage = gBattleMons[battler].maxHP / 8;
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = 1;
                    effect++;
                }
                break;/*/
            case ABILITY_HEALER:
                gBattleScripting.battler = BATTLE_PARTNER(battler);
                if (IsBattlerAlive(gBattleScripting.battler)
                    && gBattleMons[gBattleScripting.battler].status1 & STATUS1_ANY
                    && (Random() % 100) < 30)
                {
                    BattleScriptPushCursorAndCallback(BattleScript_HealerActivates);
                    effect++;
                }
                break;
            case ABILITY_SCHOOLING:
                if (gBattleMons[battler].level < 20)
                    break;
            case ABILITY_ZEN_MODE:
            case ABILITY_SHIELDS_DOWN:
                if ((effect = ShouldChangeFormHpBased(battler)))
                    BattleScriptPushCursorAndCallback(BattleScript_AttackerFormChangeEnd3);
                break;
            case ABILITY_POWER_CONSTRUCT:
                if ((gBattleMons[battler].species == SPECIES_ZYGARDE || gBattleMons[battler].species == SPECIES_ZYGARDE_10)
                    && gBattleMons[battler].hp <= gBattleMons[battler].maxHP / 2)
                {
                    gBattleStruct->changedSpecies[gBattlerPartyIndexes[battler]] = gBattleMons[battler].species;
                    gBattleMons[battler].species = SPECIES_ZYGARDE_COMPLETE;
                    BattleScriptPushCursorAndCallback(BattleScript_AttackerFormChangeEnd3);
                    effect++;
                }
                break;
            }
        }
        break;
    case ABILITYEFFECT_MOVES_BLOCK: // 2
        if ((gLastUsedAbility == ABILITY_SOUNDPROOF && gBattleMoves[move].flags & FLAG_SOUND && !(gBattleMoves[move].target & MOVE_TARGET_USER))
            || (gLastUsedAbility == ABILITY_BULLETPROOF && gBattleMoves[move].flags & FLAG_BALLISTIC))
        {
            if (gBattleMons[gBattlerAttacker].status2 & STATUS2_MULTIPLETURNS)
                gHitMarker |= HITMARKER_NO_PPDEDUCT;
            gBattlescriptCurrInstr = BattleScript_SoundproofProtected;
            effect = 1;
        }
        else if ((((gLastUsedAbility == ABILITY_DAZZLING || gLastUsedAbility == ABILITY_QUEENLY_MAJESTY
                   || (IsBattlerAlive(battler ^= BIT_FLANK)
                       && ((GetBattlerAbility(battler) == ABILITY_DAZZLING) || GetBattlerAbility(battler) == ABILITY_QUEENLY_MAJESTY)))
                   ))
                 && GetChosenMovePriority(gBattlerAttacker) > 0
                 && GetBattlerSide(gBattlerAttacker) != GetBattlerSide(battler))
        {
            if (gBattleMons[gBattlerAttacker].status2 & STATUS2_MULTIPLETURNS)
                gHitMarker |= HITMARKER_NO_PPDEDUCT;
            gBattlescriptCurrInstr = BattleScript_DazzlingProtected;
            effect = 1;
        }
		else if (gLastUsedAbility == ABILITY_ICE_FACE && IS_BATTLER_MOVE_PHYSICAL(move, gBattlerAttacker) && gBattleMons[gBattlerTarget].species == SPECIES_EISCUE)
        {
            if (gBattleMons[gBattlerAttacker].status2 & STATUS2_MULTIPLETURNS)
                gHitMarker |= HITMARKER_NO_PPDEDUCT;
            gBattlescriptCurrInstr = BattleScript_NullifyDamage;
            effect = 1;
        }
        break;
    case ABILITYEFFECT_ABSORBING: // 3
        if (move != MOVE_NONE)
        {
            u8 statId;
            switch (gLastUsedAbility)
            {
            case ABILITY_WIND_RIDER:
                if (gBattleMoves[move].flags & FLAG_WIND_BASED)
                    effect = 2, statId = STAT_ATK;
                break;
            case ABILITY_VOLT_ABSORB:
                if (moveType == TYPE_ELECTRIC)
                    effect = 1;
                break;
            case ABILITY_WATER_ABSORB:
            case ABILITY_DRY_SKIN:
                if (moveType == TYPE_WATER)
                    effect = 1;
                break;
            case ABILITY_INSECT_EATER:
                if (moveType == TYPE_BUG)
                    effect = 2, statId = STAT_ATK;
                break;
            case ABILITY_EARTH_EATER:
                if (moveType == TYPE_GROUND)
                    effect = 1;
                break;
            case ABILITY_STEEL_EATER:
                if (moveType == TYPE_STEEL)
                    effect = 2, statId = STAT_DEF;
                break;
            case ABILITY_MOTOR_DRIVE:
                if (moveType == TYPE_ELECTRIC)
                    effect = 2, statId = STAT_SPEED;
                break;
            case ABILITY_LIGHTNING_ROD:
                if (moveType == TYPE_ELECTRIC)
                    effect = 2, statId = STAT_SPATK;
                break;
            case ABILITY_STORM_DRAIN:
                if (moveType == TYPE_WATER)
                    effect = 2, statId = STAT_SPATK;
                break;
			case ABILITY_WATER_COMPACTION:
				if (moveType == TYPE_WATER)
                    effect = 2, statId = STAT_DEF;
                break;
            case ABILITY_SAP_SIPPER:
                if (moveType == TYPE_GRASS)
                    effect = 2, statId = STAT_ATK;
                break;
            case ABILITY_FLASH_FIRE:
                if (moveType == TYPE_FIRE && !((gBattleMons[battler].status1 & STATUS1_FREEZE) && B_FLASH_FIRE_FROZEN <= GEN_4))
                {
                    if (!(gBattleResources->flags->flags[battler] & RESOURCE_FLAG_FLASH_FIRE))
                    {
                        gBattleCommunication[MULTISTRING_CHOOSER] = 0;
                        if (gProtectStructs[gBattlerAttacker].notFirstStrike)
                            gBattlescriptCurrInstr = BattleScript_FlashFireBoost;
                        else
                            gBattlescriptCurrInstr = BattleScript_FlashFireBoost_PPLoss;

                        gBattleResources->flags->flags[battler] |= RESOURCE_FLAG_FLASH_FIRE;
                        effect = 3;
                    }
                    else
                    {
                        gBattleCommunication[MULTISTRING_CHOOSER] = 1;
                        if (gProtectStructs[gBattlerAttacker].notFirstStrike)
                            gBattlescriptCurrInstr = BattleScript_FlashFireBoost;
                        else
                            gBattlescriptCurrInstr = BattleScript_FlashFireBoost_PPLoss;

                        effect = 3;
                    }
                }
                break;
            }

            if (effect == 1) // Drain Hp ability.
            {
                if (BATTLER_MAX_HP(battler) || gStatuses3[battler] & STATUS3_HEAL_BLOCK)
                {
                    if ((gProtectStructs[gBattlerAttacker].notFirstStrike))
                        gBattlescriptCurrInstr = BattleScript_MonMadeMoveUseless;
                    else
                        gBattlescriptCurrInstr = BattleScript_MonMadeMoveUseless_PPLoss;
                }
                else
                {
                    if (gProtectStructs[gBattlerAttacker].notFirstStrike)
                        gBattlescriptCurrInstr = BattleScript_MoveHPDrain;
                    else
                        gBattlescriptCurrInstr = BattleScript_MoveHPDrain_PPLoss;

                    gBattleMoveDamage = gBattleMons[battler].maxHP / 4;
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = 1;
                    gBattleMoveDamage *= -1;
                }
            }
            else if (effect == 2) // Boost Stat ability;
            {
                if (gBattleMons[battler].statStages[statId] == 0xC)
                {
                    if ((gProtectStructs[gBattlerAttacker].notFirstStrike))
                        gBattlescriptCurrInstr = BattleScript_MonMadeMoveUseless;
                    else
                        gBattlescriptCurrInstr = BattleScript_MonMadeMoveUseless_PPLoss;
                }
                else
                {
                    if (gProtectStructs[gBattlerAttacker].notFirstStrike)
                        gBattlescriptCurrInstr = BattleScript_MoveStatDrain;
                    else
                        gBattlescriptCurrInstr = BattleScript_MoveStatDrain_PPLoss;

                    SET_STATCHANGER(statId, 1, FALSE);
                    gBattleMons[battler].statStages[statId]++;
                    PREPARE_STAT_BUFFER(gBattleTextBuff1, statId);
                }
            }
        }
        break;
    case ABILITYEFFECT_MOVE_END: // Think contact abilities.
        switch (gLastUsedAbility)
        {
        case ABILITY_JUSTIFIED:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && TARGET_TURN_DAMAGED
             && IsBattlerAlive(battler)
             && (moveType == TYPE_DARK || moveType == TYPE_GHOST || moveType == TYPE_DRAGON)
             && gBattleMons[battler].statStages[GetHigestAttackingStatFromBattler(battler)] != 12)
            {
                SET_STATCHANGER(GetHigestAttackingStatFromBattler(battler), 2, FALSE);
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_TargetAbilityStatRaise;
                effect++;
            }
            break;
        case ABILITY_RATTLED:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && TARGET_TURN_DAMAGED
             && IsBattlerAlive(battler)
             && (moveType == TYPE_DARK || moveType == TYPE_BUG || moveType == TYPE_GHOST)
             && gBattleMons[battler].statStages[STAT_SPEED] != 12)
            {
                SET_STATCHANGER(STAT_SPEED, 1, FALSE);
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_TargetAbilityStatRaise;
                effect++;
            }
            break;
        case ABILITY_STAMINA:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && TARGET_TURN_DAMAGED
             && IsBattlerAlive(battler)
             && gBattleMons[battler].statStages[STAT_DEF] != 12)
            {
                SET_STATCHANGER(STAT_DEF, 1, FALSE);
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_TargetAbilityStatRaise;
                effect++;
            }
            break;
        case ABILITY_BERSERK:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && TARGET_TURN_DAMAGED
             && IsBattlerAlive(battler)
            // Had more than half of hp before, now has less
             && gBattleStruct->hpBefore[battler] > gBattleMons[battler].maxHP / 2
             && gBattleMons[battler].hp < gBattleMons[battler].maxHP / 2
             && (gMultiHitCounter == 0 || gMultiHitCounter == 1)
             && !(GetBattlerAbility(gBattlerAttacker) == ABILITY_SHEER_FORCE && gBattleMoves[gCurrentMove].flags & FLAG_SHEER_FORCE_BOOST)
             && gBattleMons[battler].statStages[STAT_SPATK] != 12)
            {
                SET_STATCHANGER(STAT_SPATK, 1, FALSE);
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_TargetAbilityStatRaise;
                effect++;
            }
            break;
        case ABILITY_EMERGENCY_EXIT:
        case ABILITY_WIMP_OUT:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && TARGET_TURN_DAMAGED
             && IsBattlerAlive(battler)
            // Had more than half of hp before, now has less
             && gBattleStruct->hpBefore[battler] > gBattleMons[battler].maxHP / 2
             && gBattleMons[battler].hp < gBattleMons[battler].maxHP / 2
             && (gMultiHitCounter == 0 || gMultiHitCounter == 1)
             && !(GetBattlerAbility(gBattlerAttacker) == ABILITY_SHEER_FORCE && gBattleMoves[gCurrentMove].flags & FLAG_SHEER_FORCE_BOOST)
             && (CanBattlerSwitch(battler) || !(gBattleTypeFlags & BATTLE_TYPE_TRAINER))
             && !(gBattleTypeFlags & BATTLE_TYPE_ARENA)
             && CountUsablePartyMons(battler) > 0)
            {
                gBattleResources->flags->flags[battler] |= RESOURCE_FLAG_EMERGENCY_EXIT;
                effect++;
            }
            break;
        case ABILITY_WEAK_ARMOR:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && TARGET_TURN_DAMAGED
             && IsBattlerAlive(battler)
             && IS_BATTLER_MOVE_PHYSICAL(gCurrentMove, gBattlerAttacker)
             && (gBattleMons[battler].statStages[STAT_SPEED] != 12 || gBattleMons[battler].statStages[STAT_DEF] != 0))
            {
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_WeakArmorActivates;
                effect++;
            }
            break;
        case ABILITY_CURSED_BODY:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && TARGET_TURN_DAMAGED
             && gDisableStructs[gBattlerAttacker].disabledMove == MOVE_NONE
             && IsBattlerAlive(gBattlerAttacker)
             && !IsAbilityOnSide(gBattlerAttacker, ABILITY_AROMA_VEIL)
             && gBattleMons[gBattlerAttacker].pp[gChosenMovePos] != 0
             && (Random() % 3) == 0)
            {
                gDisableStructs[gBattlerAttacker].disabledMove = gChosenMove;
                gDisableStructs[gBattlerAttacker].disableTimer = 4;
                PREPARE_MOVE_BUFFER(gBattleTextBuff1, gChosenMove);
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_CursedBodyActivates;
                effect++;
            }
            break;
        case ABILITY_MUMMY:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && IsBattlerAlive(gBattlerAttacker)
             && (gBattleMoves[move].flags & FLAG_MAKES_CONTACT))
            {
                switch (gBattleMons[gBattlerAttacker].ability)
                {
                case ABILITY_MUMMY:
                case ABILITY_BATTLE_BOND:
                case ABILITY_COMATOSE:
                case ABILITY_DISGUISE:
                case ABILITY_MULTITYPE:
                case ABILITY_POWER_CONSTRUCT:
                case ABILITY_RKS_SYSTEM:
                case ABILITY_SCHOOLING:
                case ABILITY_SHIELDS_DOWN:
                case ABILITY_STANCE_CHANGE:
                    break;
                default:
                    gLastUsedAbility = gBattleMons[gBattlerAttacker].ability = ABILITY_MUMMY;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_MummyActivates;
                    effect++;
                    break;
                }
            }
            break;
        case ABILITY_WANDERING_SPIRIT:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && IsBattlerAlive(gBattlerAttacker)
             && TARGET_TURN_DAMAGED
             && (gBattleMoves[move].flags & FLAG_MAKES_CONTACT))
            {
                switch (gBattleMons[gBattlerAttacker].ability)
                {
                case ABILITY_DISGUISE:
                case ABILITY_FLOWER_GIFT:
                case ABILITY_GULP_MISSILE:
                case ABILITY_HUNGER_SWITCH:
                case ABILITY_ICE_FACE:
                case ABILITY_ILLUSION:
                case ABILITY_IMPOSTER:
                case ABILITY_RECEIVER:
                case ABILITY_RKS_SYSTEM:
                case ABILITY_SCHOOLING:
                case ABILITY_STANCE_CHANGE:
                case ABILITY_WONDER_GUARD:
                case ABILITY_ZEN_MODE:
                    break;
                default:
                    gLastUsedAbility = gBattleMons[gBattlerAttacker].ability;
                    gBattleMons[gBattlerAttacker].ability = gBattleMons[gBattlerTarget].ability;
                    gBattleMons[gBattlerTarget].ability = gLastUsedAbility;
                    RecordAbilityBattle(gBattlerAttacker, gBattleMons[gBattlerAttacker].ability);
                    RecordAbilityBattle(gBattlerTarget, gBattleMons[gBattlerTarget].ability);
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_WanderingSpiritActivates;
                    effect++;
                    break;
                }
            }
            break;
        case ABILITY_ANGER_POINT:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && TARGET_TURN_DAMAGED
             && IsBattlerAlive(battler)
             && IS_MOVE_PHYSICAL(gCurrentMove)
             && gBattleMons[battler].statStages[GetHigestAttackingStatFromBattler(battler)] != 12)
            {
                SET_STATCHANGER(GetHigestAttackingStatFromBattler(battler), 1, FALSE);
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_TargetAbilityStatRaise;
                effect++;
            }
            break;
        /*/case ABILITY_COLOR_CHANGE:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && move != MOVE_STRUGGLE
             && gBattleMoves[move].power != 0
             && TARGET_TURN_DAMAGED
             && !IS_BATTLER_OF_TYPE(battler, moveType)
             && gBattleMons[battler].hp != 0)
            {
                SET_BATTLER_TYPE(battler, moveType);
                PREPARE_TYPE_BUFFER(gBattleTextBuff1, moveType);
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_ColorChangeActivates;
                effect++;
            }
            break;/*/
        case ABILITY_GOOEY:
        case ABILITY_TANGLING_HAIR:
        case ABILITY_HONEY_GATHER:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && gBattleMons[gBattlerAttacker].hp != 0
             && gBattleMons[gBattlerAttacker].statStages[STAT_SPEED] != 0
             && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
             && TARGET_TURN_DAMAGED
             && IsMoveMakingContact(move, gBattlerAttacker))
            {
                gBattleScripting.moveEffect = MOVE_EFFECT_AFFECTS_USER | MOVE_EFFECT_SPD_MINUS_1;
                PREPARE_ABILITY_BUFFER(gBattleTextBuff1, gLastUsedAbility);
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_AbilityStatusEffect;
                gHitMarker |= HITMARKER_IGNORE_SAFEGUARD;
                effect++;
            }
            break;
        case ABILITY_ROUGH_SKIN:
        case ABILITY_IRON_BARBS:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && gBattleMons[gBattlerAttacker].hp != 0
             && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
             && TARGET_TURN_DAMAGED
             && IsMoveMakingContact(move, gBattlerAttacker))
            {
                gBattleMoveDamage = gBattleMons[gBattlerAttacker].maxHP / 8;
                if (gBattleMoveDamage == 0)
                    gBattleMoveDamage = 1;
                PREPARE_ABILITY_BUFFER(gBattleTextBuff1, gLastUsedAbility);
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_RoughSkinActivates;
                effect++;
            }
            break;
        case ABILITY_HOLLOW_BODY:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && gBattleMons[gBattlerAttacker].hp != 0
             && gBattleMons[gBattlerAttacker].ability != ABILITY_SOUNDPROOF
             && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
             && TARGET_TURN_DAMAGED)
            {
                gBattleMoveDamage = gBattleMons[gBattlerAttacker].maxHP / 10;
                if (gBattleMoveDamage == 0)
                    gBattleMoveDamage = 1;
                PREPARE_ABILITY_BUFFER(gBattleTextBuff1, gLastUsedAbility);
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_RoughSkinActivates;
                effect++;
            }
            break;
        case ABILITY_AFTERMATH:
            if (!IsAbilityOnField(ABILITY_DAMP)
             && !(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && gBattleMons[gBattlerTarget].hp == 0
             && IsBattlerAlive(gBattlerAttacker)
             && IsMoveMakingContact(move, gBattlerAttacker))
            {
                gBattleMoveDamage = gBattleMons[gBattlerAttacker].maxHP / 4;
                if (gBattleMoveDamage == 0)
                    gBattleMoveDamage = 1;
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_AftermathDmg;
                effect++;
            }
            break;
        case ABILITY_INNARDS_OUT:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && gBattleMons[gBattlerTarget].hp == 0
             && IsBattlerAlive(gBattlerAttacker))
            {
                gBattleMoveDamage = gSpecialStatuses[gBattlerTarget].dmg;
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_AftermathDmg;
                effect++;
            }
            break;
        case ABILITY_EFFECT_SPORE:
            if (!IS_BATTLER_OF_TYPE(gBattlerAttacker, TYPE_GRASS)
             && GetBattlerAbility(gBattlerAttacker) != ABILITY_OVERCOAT
             && GetBattlerHoldEffect(gBattlerAttacker, TRUE) != HOLD_EFFECT_SAFETY_GOOGLES)
            {
                i = Random() % 3;
                if (i == 0)
                    goto POISON_POINT;
                if (i == 1)
                    goto STATIC;
                // Sleep
                if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                 && gBattleMons[gBattlerAttacker].hp != 0
                 && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                 && TARGET_TURN_DAMAGED
                 && GetBattlerAbility(gBattlerAttacker) != ABILITY_INSOMNIA
                 && GetBattlerAbility(gBattlerAttacker) != ABILITY_VITAL_SPIRIT
                 && !(gBattleMons[gBattlerAttacker].status1 & STATUS1_ANY)
                 && !IsAbilityStatusProtected(gBattlerAttacker)
                 && IsMoveMakingContact(move, gBattlerAttacker)
                 && (Random() % 3) == 0)
                {
                    gBattleScripting.moveEffect = MOVE_EFFECT_AFFECTS_USER | MOVE_EFFECT_SLEEP;
                    PREPARE_ABILITY_BUFFER(gBattleTextBuff1, gLastUsedAbility);
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_AbilityStatusEffect;
                    gHitMarker |= HITMARKER_IGNORE_SAFEGUARD;
                    effect++;
                }
            }
            break;
        POISON_POINT:
        case ABILITY_POISON_POINT:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && gBattleMons[gBattlerAttacker].hp != 0
             && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
             && TARGET_TURN_DAMAGED
             && !IS_BATTLER_OF_TYPE(gBattlerAttacker, TYPE_POISON)
             && !IS_BATTLER_OF_TYPE(gBattlerAttacker, TYPE_STEEL)
             && CanBePoisoned(gBattlerAttacker, gBattlerTarget)
             && !(gBattleMons[gBattlerAttacker].status1 & STATUS1_ANY)
             && !IsAbilityStatusProtected(gBattlerAttacker)
             && IsMoveMakingContact(move, gBattlerAttacker)
             && (Random() % 3) == 0)
            {
                gBattleScripting.moveEffect = MOVE_EFFECT_AFFECTS_USER | MOVE_EFFECT_POISON;
                PREPARE_ABILITY_BUFFER(gBattleTextBuff1, gLastUsedAbility);
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_AbilityStatusEffect;
                gHitMarker |= HITMARKER_IGNORE_SAFEGUARD;
                effect++;
            }
            break;
        STATIC:
        case ABILITY_STATIC:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && gBattleMons[gBattlerAttacker].hp != 0
             && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
             && TARGET_TURN_DAMAGED
             && CanParalyzeType(gBattlerTarget, gBattlerAttacker)
             && GetBattlerAbility(gBattlerAttacker) != ABILITY_LIMBER
             && !(gBattleMons[gBattlerAttacker].status1 & STATUS1_ANY)
             && !IsAbilityStatusProtected(gBattlerAttacker)
             && IsMoveMakingContact(move, gBattlerAttacker)
             && (Random() % 3) == 0)
            {
                gBattleScripting.moveEffect = MOVE_EFFECT_AFFECTS_USER | MOVE_EFFECT_PARALYSIS;
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_AbilityStatusEffect;
                gHitMarker |= HITMARKER_IGNORE_SAFEGUARD;
                effect++;
            }
            break;
        case ABILITY_FLAME_BODY:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && gBattleMons[gBattlerAttacker].hp != 0
             && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
             && (gBattleMoves[move].flags & FLAG_MAKES_CONTACT)
             && TARGET_TURN_DAMAGED
             && !IS_BATTLER_OF_TYPE(gBattlerAttacker, TYPE_FIRE)
             && GetBattlerAbility(gBattlerAttacker) != ABILITY_WATER_VEIL
			 && !(gFieldStatuses & STATUS_FIELD_WATERSPORT)
             && !(gBattleMons[gBattlerAttacker].status1 & STATUS1_ANY)
             && !IsAbilityStatusProtected(gBattlerAttacker)
             && (Random() % 3) == 0)
            {
                gBattleScripting.moveEffect = MOVE_EFFECT_AFFECTS_USER | MOVE_EFFECT_BURN;
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_AbilityStatusEffect;
                gHitMarker |= HITMARKER_IGNORE_SAFEGUARD;
                effect++;
            }
            break;
        case ABILITY_CUTE_CHARM:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && gBattleMons[gBattlerAttacker].hp != 0
             && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
             && (gBattleMoves[move].flags & FLAG_MAKES_CONTACT)
             && TARGET_TURN_DAMAGED
             && gBattleMons[gBattlerTarget].hp != 0
             && (Random() % 3) == 0
             && GetBattlerAbility(gBattlerAttacker) != ABILITY_OBLIVIOUS
             && !IsAbilityOnSide(gBattlerAttacker, ABILITY_AROMA_VEIL)
             && GetGenderFromSpeciesAndPersonality(speciesAtk, pidAtk) != GetGenderFromSpeciesAndPersonality(speciesDef, pidDef)
             && !(gBattleMons[gBattlerAttacker].status2 & STATUS2_INFATUATION)
             && GetGenderFromSpeciesAndPersonality(speciesAtk, pidAtk) != MON_GENDERLESS
             && GetGenderFromSpeciesAndPersonality(speciesDef, pidDef) != MON_GENDERLESS)
            {
                gBattleMons[gBattlerAttacker].status2 |= STATUS2_INFATUATED_WITH(gBattlerTarget);
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_CuteCharmActivates;
                effect++;
            }
            break;
        case ABILITY_COTTON_DOWN:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && gBattleMons[gBattlerAttacker].hp != 0
             && IsBattlerAlive(gBattlerAttacker)
             && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
             && gBattlerAttacker != gBattlerTarget
             && TARGET_TURN_DAMAGED)
            {
                gEffectBattler = gBattlerTarget;
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_CottonDownActivates;
                effect++;
            }
            break;
        case ABILITY_STEAM_ENGINE:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && TARGET_TURN_DAMAGED
             && IsBattlerAlive(battler)
             && gBattleMons[battler].statStages[STAT_SPEED] != 12
             && (moveType == TYPE_FIRE || moveType == TYPE_WATER))
            {
                SET_STATCHANGER(STAT_SPEED, 6, FALSE);
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_TargetAbilityStatRaise;
                effect++;
            }
            break;
        case ABILITY_SAND_SPIT:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
             && TARGET_TURN_DAMAGED
             && !(gBattleWeather & WEATHER_SANDSTORM_ANY && WEATHER_HAS_EFFECT))
            {
                if (TryChangeBattleWeather(battler, ENUM_WEATHER_SANDSTORM, TRUE))
                {
                    gBattleScripting.battler = gActiveBattler = battler;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_SandSpitActivates;
                    effect++;
                }
            }
            break;
        case ABILITY_SHOOTING_STAR:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT) && 
                TARGET_TURN_DAMAGED && 
                gDisableStructs[gBattlerTarget].abilityEffectDone == 0 &&
				gWishFutureKnock.wishCounter[gBattlerTarget] == 0 &&
                gBattleMons[gBattlerTarget].hp < (gBattleMons[gBattlerTarget].maxHP / 4) &&
                gBattleMons[gBattlerTarget].hp != 0)
            {
				gBattlerAttacker = battler;
                gDisableStructs[gBattlerTarget].abilityEffectDone = 255; // Only once per switch in
                gWishFutureKnock.wishCounter[gBattlerTarget] = 2;
                BattleScriptPushCursorAndCallback(BattleScript_ShootingStarActivate); // Try activate
                effect++;
            }
			break;
		case ABILITY_LOOSE_QUILLS:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && gBattleMons[gBattlerAttacker].hp != 0
             && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
             && TARGET_TURN_DAMAGED
             && IsMoveMakingContact(move, gBattlerAttacker))
            {
                u8 targetSide = GetBattlerSide(gBattlerAttacker);
                gSideStatuses[targetSide] |= SIDE_STATUS_SPIKES;
                gSideTimers[targetSide].spikesAmount++;

                gBattlerAttacker = gBattlerTarget;
                PREPARE_ABILITY_BUFFER(gBattleTextBuff1, gLastUsedAbility);
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_EffectLooseQuills;
                effect++;
            }
            break;
        case ABILITY_ILLUSION:
            if (gBattleStruct->illusion[gBattlerTarget].on && !gBattleStruct->illusion[gBattlerTarget].broken && TARGET_TURN_DAMAGED)
            {
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_IllusionOff;
                effect++;
            }
            break;
        case ABILITY_PERISH_BODY:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
             && TARGET_TURN_DAMAGED
             && IsBattlerAlive(battler)
             && IsMoveMakingContact(move, gBattlerAttacker)
             && !(gStatuses3[gBattlerAttacker] & STATUS3_PERISH_SONG))
            {
                if (!(gStatuses3[battler] & STATUS3_PERISH_SONG))
                {
                    gStatuses3[battler] |= STATUS3_PERISH_SONG;
                    gDisableStructs[battler].perishSongTimer = 3;
                    gDisableStructs[battler].perishSongTimerStartValue = 3;
                }
                gStatuses3[gBattlerAttacker] |= STATUS3_PERISH_SONG;
                gDisableStructs[gBattlerAttacker].perishSongTimer = 3;
                gDisableStructs[gBattlerAttacker].perishSongTimerStartValue = 3;
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_PerishBodyActivates;
                effect++;
            }
            break;
		case ABILITY_GULP_MISSILE:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
             && TARGET_TURN_DAMAGED
             && IsBattlerAlive(battler)
             && gBattleMons[battler].species == SPECIES_CRAMORANT_GORGING)
            {
                gBattleStruct->changedSpecies[gBattlerPartyIndexes[battler]] = gBattleMons[battler].species;
                gBattleMons[battler].species = SPECIES_CRAMORANT;
                gBattleMoveDamage = gBattleMons[gBattlerAttacker].maxHP / 4;
                if (gBattleMoveDamage == 0)
                    gBattleMoveDamage = 1;
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_GulpMissileGorging;
                effect++;
            }
            else if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
             && TARGET_TURN_DAMAGED
             && IsBattlerAlive(battler)
             && gBattleMons[battler].species == SPECIES_CRAMORANT_GULPING)
            {
                gBattleStruct->changedSpecies[gBattlerPartyIndexes[battler]] = gBattleMons[battler].species;
                gBattleMons[battler].species = SPECIES_CRAMORANT;
                gBattleMoveDamage = gBattleMons[gBattlerAttacker].maxHP / 4;
                if (gBattleMoveDamage == 0)
                    gBattleMoveDamage = 1;
                SET_STATCHANGER(STAT_DEF, 1, TRUE);
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_GulpMissileGulping;
                effect++;
            }
            break;
		case ABILITY_ICE_FACE:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
             && IsBattlerAlive(gBattlerTarget)
             && IS_BATTLER_MOVE_PHYSICAL(gCurrentMove, gBattlerAttacker)
             && gBattleMons[gBattlerTarget].species == SPECIES_EISCUE)
            {
                gBattleStruct->changedSpecies[gBattlerPartyIndexes[gBattlerTarget]] = gBattleMons[gBattlerTarget].species;
                gBattleMons[gBattlerTarget].species = SPECIES_EISCUE_NOICE_FACE;
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_TargetFormChangeWithString;
                effect++;
            }
            break;
        }
        break;
    case ABILITYEFFECT_MOVE_END_ATTACKER: // Same as above, but for attacker
        switch (gLastUsedAbility)
        {
        case ABILITY_POISON_TOUCH:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && gBattleMons[gBattlerTarget].hp != 0
             && !gProtectStructs[gBattlerTarget].confusionSelfDmg
             && !IS_BATTLER_OF_TYPE(gBattlerTarget, TYPE_POISON)
             && !IS_BATTLER_OF_TYPE(gBattlerTarget, TYPE_STEEL)
             && GetBattlerAbility(gBattlerTarget) != ABILITY_IMMUNITY
             && !(gBattleMons[gBattlerTarget].status1 & STATUS1_ANY)
             && !IsAbilityStatusProtected(gBattlerTarget)
             && IsMoveMakingContact(move, gBattlerAttacker)
             && (Random() % 3) == 0)
            {
                gBattleScripting.moveEffect = MOVE_EFFECT_POISON;
                PREPARE_ABILITY_BUFFER(gBattleTextBuff1, gLastUsedAbility);
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_AbilityStatusEffect;
                gHitMarker |= HITMARKER_IGNORE_SAFEGUARD;
                effect++;
            }
            break;
		case ABILITY_TOTTERING_STEP:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && gBattleMons[gBattlerTarget].hp != 0
             && !gProtectStructs[gBattlerTarget].confusionSelfDmg
             && GetBattlerAbility(gBattlerTarget) != ABILITY_OWN_TEMPO
             && !(gBattleMons[gBattlerTarget].status2 & STATUS2_CONFUSION)
             && !IsAbilityStatusProtected(gBattlerTarget)
             && IsMoveMakingContact(move, gBattlerAttacker)
             && (Random() % 3) == 0)
            {
                gBattleScripting.moveEffect = MOVE_EFFECT_CONFUSION;
                PREPARE_ABILITY_BUFFER(gBattleTextBuff1, gLastUsedAbility);
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_AbilityStatusEffect;
                gHitMarker |= HITMARKER_IGNORE_SAFEGUARD;
                effect++;
            }
            break;
		case ABILITY_GULP_MISSILE:
            if ((effect = ShouldChangeFormHpBased(battler))
             && (gCurrentMove == MOVE_SURF
             || gStatuses3[battler] & STATUS3_UNDERWATER))
            {
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_AttackerFormChange;
                effect++;
            }
            break;
        case ABILITY_COILED_UP:
            if ((gCurrentMove == MOVE_COIL || gCurrentMove == MOVE_GLARE) && 
               !(gStatuses4[battler] & STATUS4_COILED_UP)){
				gBattlerAttacker = battler;
				gStatuses4[battler] = STATUS4_COILED_UP;
                BattleScriptPushCursorAndCallback(BattleScript_EffectCoiledUp); // Try activate
                effect++;
            }
            break;
		case ABILITY_PARENTAL_BOND:
            if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
             && gBattleMons[gBattlerTarget].hp != 0
             && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
             && TARGET_TURN_DAMAGED)
            {
                //gBattleMoveDamage = gBattleMons[gBattlerTarget].maxHP / 16;
				gBattleMoveDamage = VarGet(VAR_LAST_DAMAGE_DONE)/4;
				
                if (gBattleMoveDamage == 0)
                    gBattleMoveDamage = 1;
				
				gLastUsedAbility = ABILITY_PARENTAL_BOND;
                PREPARE_ABILITY_BUFFER(gBattleTextBuff1, gLastUsedAbility);
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_AttackerSecondHitActivates;
                effect++;
            }
            break;
		case ABILITY_BATTLE_BOND:
            if (gBattleMons[gBattlerAttacker].species != SPECIES_GRENINJA_ASH
             && gBattleResults.opponentFaintCounter != 0
             && CalculateEnemyPartyCount() > 1)
            {
                PREPARE_SPECIES_BUFFER(gBattleTextBuff1, gBattleMons[gBattlerAttacker].species);
                gBattleStruct->changedSpecies[gBattlerPartyIndexes[gBattlerAttacker]] = gBattleMons[gBattlerAttacker].species;
                gBattleMons[gBattlerAttacker].species = SPECIES_GRENINJA_ASH;
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_BattleBondActivatesOnMoveEndAttacker;
            }
            break;
        }

		if (gSignatureMoveList[GetFormSpeciesId(gBattleMons[gBattlerAttacker].species, gBattleMons[gBattlerAttacker].formId)].move == move){
            //Set Hazards
            bool8 setHazards = FALSE;
            u8 hazardType = FIELD_EFFECT_NONE;
            u8 hazardChance = 0;
            //Ups Attacker Stat
            bool8 statUp = FALSE;
            u8 statUpId = 0;
            u8 statUpChance = 0;
            //Lowers Target Stat
            bool8 statDown = FALSE;
            u8 statDownId = 0;
            u8 statDownChance = 0;
            //Set Status to the Target
            bool8 setStatus = FALSE;
            u8 setStatusId = 0;
            u8 setStatusChance = 0;
            u8 setStatusargument = 0;
            u16 speciesId = GetFormSpeciesId(gBattleMons[gBattlerAttacker].species, gBattleMons[gBattlerAttacker].formId);
                
            //Hazards
            if(gSignatureMoveList[speciesId].move == move){
                if(gSignatureMoveList[speciesId].modification == SIGNATURE_MOD_MODIFY_FIELD){
                    hazardType = gSignatureMoveList[speciesId].variable;
                    hazardChance = gSignatureMoveList[speciesId].chance;
                    setHazards = TRUE;
                }
                else if (gSignatureMoveList[speciesId].modification2 == SIGNATURE_MOD_MODIFY_FIELD){
                    hazardType = gSignatureMoveList[speciesId].variable2;
                    hazardChance = gSignatureMoveList[speciesId].chance2;
                    setHazards = TRUE;
                }
                else if (gSignatureMoveList[speciesId].modification3 == SIGNATURE_MOD_MODIFY_FIELD){
                    hazardType = gSignatureMoveList[speciesId].variable3;
                    hazardChance = gSignatureMoveList[speciesId].chance3;
                    setHazards = TRUE;
                    }
                else if (gSignatureMoveList[speciesId].modification4 == SIGNATURE_MOD_MODIFY_FIELD){
                    hazardType = gSignatureMoveList[speciesId].variable4;
                    hazardChance = gSignatureMoveList[speciesId].chance4;
                    setHazards = TRUE;
                }
                else if (gSignatureMoveList[speciesId].modification5 == SIGNATURE_MOD_MODIFY_FIELD){
                    hazardType = gSignatureMoveList[speciesId].variable5;
                    hazardChance = gSignatureMoveList[speciesId].chance5;
                    setHazards = TRUE;
                }
                else if (gSignatureMoveList[speciesId].modification6 == SIGNATURE_MOD_MODIFY_FIELD){
                    hazardType = gSignatureMoveList[speciesId].variable6;
                    hazardChance = gSignatureMoveList[speciesId].chance6;
                    setHazards = TRUE;
                }
            }

            //If there is no set chance it means it should always happen
            if(hazardChance == 0)
                hazardChance = 100;

            if(setHazards && (Random() % 100) < hazardChance){
                switch(hazardType){
                    case FIELD_OPPONET_SET_STEALTH_ROCK:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && ((TARGET_TURN_DAMAGED) || gBattleMoves[move].split == SPLIT_STATUS)
                        && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                        && !(gSideStatuses[BATTLE_OPPOSITE(gBattlerAttacker)] & SIDE_STATUS_STEALTH_ROCK))
                        {
                            gSideStatuses[BATTLE_OPPOSITE(gBattlerAttacker)] |= SIDE_STATUS_STEALTH_ROCK;
                            BattleScriptPushCursorAndCallback(BattleScript_SignatureMoveSetStealthRock);
                            gBattleScripting.battler = battler;
                            effect++;
                        }
                    break;
                    case FIELD_OPPONET_SET_STICKY_WEB:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && ((TARGET_TURN_DAMAGED) || gBattleMoves[move].split == SPLIT_STATUS)
                        && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                        && !(gSideStatuses[BATTLE_OPPOSITE(gBattlerAttacker)] & SIDE_STATUS_STICKY_WEB))
                        {
                            gSideStatuses[BATTLE_OPPOSITE(gBattlerAttacker)] |= SIDE_STATUS_STICKY_WEB;
                            BattleScriptPushCursorAndCallback(BattleScript_SignatureMoveSetStickyWeb);
                            gBattleScripting.battler = battler;
                            effect++;
                        }
                    break;
                    case FIELD_OPPONET_SET_SPIKES:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && ((TARGET_TURN_DAMAGED) || gBattleMoves[move].split == SPLIT_STATUS)
                        && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                        && !(gSideStatuses[BATTLE_OPPOSITE(gBattlerAttacker)] & SIDE_STATUS_SPIKES)
                        && gSideTimers[BATTLE_OPPOSITE(gBattlerAttacker)].spikesAmount < 3)
                        {
                            gSideTimers[BATTLE_OPPOSITE(gBattlerAttacker)].spikesAmount++;
                            gSideStatuses[BATTLE_OPPOSITE(gBattlerAttacker)] |= SIDE_STATUS_SPIKES;
                            BattleScriptPushCursorAndCallback(BattleScript_SignatureMoveSetSpikes);
                            gBattleScripting.battler = battler;
                            effect++;
                        }
                    break;
                    case FIELD_OPPONET_SET_TOXIC_SPIKES:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && ((TARGET_TURN_DAMAGED) || gBattleMoves[move].split == SPLIT_STATUS)
                        && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                        && !(gSideStatuses[BATTLE_OPPOSITE(gBattlerAttacker)] & SIDE_STATUS_TOXIC_SPIKES)
                        && gSideTimers[BATTLE_OPPOSITE(gBattlerAttacker)].toxicSpikesAmount < 3)
                        {
                            gSideTimers[BATTLE_OPPOSITE(gBattlerAttacker)].toxicSpikesAmount++;
                            gSideStatuses[BATTLE_OPPOSITE(gBattlerAttacker)] |= SIDE_STATUS_TOXIC_SPIKES;
                            BattleScriptPushCursorAndCallback(BattleScript_SignatureMoveSetToxicSpikes);
                            gBattleScripting.battler = battler;
                            effect++;
                        }
                    break;
                    case FIELD_SELF_SET_TAILWIND:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && ((TARGET_TURN_DAMAGED) || gBattleMoves[move].split == SPLIT_STATUS)
                        && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                        && !(gSideStatuses[gBattlerAttacker] & SIDE_STATUS_TAILWIND))
                        {
                            gSideTimers[gBattlerAttacker].tailwindTimer = 3;
                            gSideStatuses[gBattlerAttacker] |= SIDE_STATUS_TAILWIND;
                            BattleScriptPushCursorAndCallback(BattleScript_SignatureMoveSetTailwind);
                            gBattleScripting.battler = battler;
                            effect++;
                        }
                    break;
                    case FIELD_SET_WEATHER_SUN:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && ((TARGET_TURN_DAMAGED) || gBattleMoves[move].split == SPLIT_STATUS)
                        && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                        && TryChangeBattleWeather(battler, ENUM_WEATHER_SUN, TRUE))
                        {
                            BattleScriptPushCursorAndCallback(BattleScript_TheWeatherBecameSunny);
                            gBattleScripting.battler = battler;
                            effect++;
                        }
                    break;
                    case FIELD_SET_WEATHER_RAIN:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && ((TARGET_TURN_DAMAGED) || gBattleMoves[move].split == SPLIT_STATUS)
                        && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                        && TryChangeBattleWeather(battler, ENUM_WEATHER_RAIN, TRUE))
                        {
                            BattleScriptPushCursorAndCallback(BattleScript_TheWeatherBecameRainy);
                            gBattleScripting.battler = battler;
                            effect++;
                        }
                    break;
                    case FIELD_SET_WEATHER_HAIL:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && ((TARGET_TURN_DAMAGED) || gBattleMoves[move].split == SPLIT_STATUS)
                        && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                        && TryChangeBattleWeather(battler, ENUM_WEATHER_HAIL, TRUE))
                        {
                            BattleScriptPushCursorAndCallback(BattleScript_TheWeatherBecameHail);
                            gBattleScripting.battler = battler;
                            effect++;
                        }
                    break;
                    case FIELD_SET_WEATHER_SANDSTORM:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && ((TARGET_TURN_DAMAGED) || gBattleMoves[move].split == SPLIT_STATUS)
                        && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                        && TryChangeBattleWeather(battler, ENUM_WEATHER_SANDSTORM, TRUE))
                        {
                            BattleScriptPushCursorAndCallback(BattleScript_TheWeatherBecameSandstorm);
                            gBattleScripting.battler = battler;
                            effect++;
                        }
                    break;
                    case FIELD_SET_TERRAIN_ELECTRIC:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && ((TARGET_TURN_DAMAGED) || gBattleMoves[move].split == SPLIT_STATUS)
                        && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                        && TryChangeBattleTerrain(gBattlerAttacker, STATUS_FIELD_ELECTRIC_TERRAIN, &gFieldTimers.electricTerrainTimer))
                        {
                            BattleScriptPushCursorAndCallback(BattleScript_SetElectricTerrain);
                            gBattleScripting.battler = battler;
                            effect++;
                        }
                    break;
                    case FIELD_SET_TERRAIN_PSYCHIC:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && ((TARGET_TURN_DAMAGED) || gBattleMoves[move].split == SPLIT_STATUS)
                        && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                        && TryChangeBattleTerrain(gBattlerAttacker, STATUS_FIELD_PSYCHIC_TERRAIN, &gFieldTimers.psychicTerrainTimer))
                        {
                            BattleScriptPushCursorAndCallback(BattleScript_SetPsychicTerrain);
                            gBattleScripting.battler = battler;
                            effect++;
                        }
                    break;
                    case FIELD_SET_TERRAIN_MISTY:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && ((TARGET_TURN_DAMAGED) || gBattleMoves[move].split == SPLIT_STATUS)
                        && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                        && TryChangeBattleTerrain(gBattlerAttacker, STATUS_FIELD_PSYCHIC_TERRAIN, &gFieldTimers.mistyTerrainTimer))
                        {
                            BattleScriptPushCursorAndCallback(BattleScript_SetMistyTerrain);
                            gBattleScripting.battler = battler;
                            effect++;
                        }
                    break;
                    case FIELD_SET_TERRAIN_GRASS:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && ((TARGET_TURN_DAMAGED) || gBattleMoves[move].split == SPLIT_STATUS)
                        && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                        && TryChangeBattleTerrain(gBattlerAttacker, STATUS_FIELD_PSYCHIC_TERRAIN, &gFieldTimers.grassyTerrainTimer))
                        {
                            BattleScriptPushCursorAndCallback(BattleScript_SetGrassyTerrain);
                            gBattleScripting.battler = battler;
                            effect++;
                        }
                    break;
                }
            }

            //Attacker Stat Up
            if(gSignatureMoveList[speciesId].move == move){
                if(gSignatureMoveList[speciesId].modification == SIGNATURE_MOD_ATTACKER_STAT_UP){
                    statUpId = gSignatureMoveList[speciesId].variable;
                    statUpChance = gSignatureMoveList[speciesId].chance;
                    statUp = TRUE;
                }
                else if (gSignatureMoveList[speciesId].modification2 == SIGNATURE_MOD_ATTACKER_STAT_UP){
                    statUpId = gSignatureMoveList[speciesId].variable2;
                    statUpChance = gSignatureMoveList[speciesId].chance2;
                    statUp = TRUE;
                }
                else if (gSignatureMoveList[speciesId].modification3 == SIGNATURE_MOD_ATTACKER_STAT_UP){
                    statUpId = gSignatureMoveList[speciesId].variable3;
                    statUpChance = gSignatureMoveList[speciesId].chance3;
                    statUp = TRUE;
                }
                else if (gSignatureMoveList[speciesId].modification4 == SIGNATURE_MOD_ATTACKER_STAT_UP){
                    statUpId = gSignatureMoveList[speciesId].variable4;
                    statUpChance = gSignatureMoveList[speciesId].chance4;
                    statUp = TRUE;
                }
                else if (gSignatureMoveList[speciesId].modification5 == SIGNATURE_MOD_ATTACKER_STAT_UP){
                    statUpId = gSignatureMoveList[speciesId].variable5;
                    statUpChance = gSignatureMoveList[speciesId].chance5;
                    statUp = TRUE;
                }
                else if (gSignatureMoveList[speciesId].modification6 == SIGNATURE_MOD_ATTACKER_STAT_UP){
                    statUpId = gSignatureMoveList[speciesId].variable6;
                    statUpChance = gSignatureMoveList[speciesId].chance6;
                    statUp = TRUE;
                }
            }

            //If there is no set chance it means it should always happen
            if(statUpChance == 0)
                statUpChance = 100;

            if (statUp && (Random() % 100) < statUpChance){
                if(gBattleMoves[move].split != SPLIT_STATUS){
                    if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                    && TARGET_TURN_DAMAGED
                    && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                    && gBattleMons[gBattlerAttacker].statStages[statUpId] != 12)
                    {
                        PREPARE_STAT_BUFFER(gBattleTextBuff1, statUpId);
                        BattleScriptPushCursor();
                        gBattleMons[battler].statStages[statUpId]++;
                        gBattleScripting.animArg1 = 14 + statUpId;
                        gBattleScripting.animArg2 = 0;
                        BattleScriptPushCursorAndCallback(BattleScript_AttackerMoveRaisedStat);
                        gBattleScripting.battler = battler;
                        effect++;
                    }
                }
                else if(gBattleMoves[move].split == SPLIT_STATUS){
                    if (gBattleMons[gBattlerAttacker].statStages[statUpId] != 12)
					{
						PREPARE_STAT_BUFFER(gBattleTextBuff1, statUpId);
						BattleScriptPushCursor();
						gBattleMons[battler].statStages[statUpId]++;
						gBattleScripting.animArg1 = 14 + statUpId;
						gBattleScripting.animArg2 = 0;
						BattleScriptPushCursorAndCallback(BattleScript_AttackerMoveRaisedStat);
						gBattleScripting.battler = battler;
						effect++;
					}
                }
			}

            //Target Stat Down
            if(gSignatureMoveList[speciesId].move == move){
                if(gSignatureMoveList[speciesId].modification == SIGNATURE_MOD_TARGET_STAT_DOWN){
                    statDownId = gSignatureMoveList[speciesId].variable;
                    statDownChance = gSignatureMoveList[speciesId].chance;
                    statDown = TRUE;
                }
                else if (gSignatureMoveList[speciesId].modification2 == SIGNATURE_MOD_TARGET_STAT_DOWN){
                    statDownId = gSignatureMoveList[speciesId].variable2;
                    statDownChance = gSignatureMoveList[speciesId].chance2;
                    statDown = TRUE;
                }
                else if (gSignatureMoveList[speciesId].modification3 == SIGNATURE_MOD_TARGET_STAT_DOWN){
                    statDownId = gSignatureMoveList[speciesId].variable3;
                    statDownChance = gSignatureMoveList[speciesId].chance3;
                    statDown = TRUE;
                }
                else if (gSignatureMoveList[speciesId].modification4 == SIGNATURE_MOD_TARGET_STAT_DOWN){
                    statDownId = gSignatureMoveList[speciesId].variable4;
                    statDownChance = gSignatureMoveList[speciesId].chance4;
                    statDown = TRUE;
                }
                else if (gSignatureMoveList[speciesId].modification5 == SIGNATURE_MOD_TARGET_STAT_DOWN){
                    statDownId = gSignatureMoveList[speciesId].variable5;
                    statDownChance = gSignatureMoveList[speciesId].chance5;
                    statDown = TRUE;
                }
                else if (gSignatureMoveList[speciesId].modification6 == SIGNATURE_MOD_TARGET_STAT_DOWN){
                    statDownId = gSignatureMoveList[speciesId].variable6;
                    statDownChance = gSignatureMoveList[speciesId].chance6;
                    statDown = TRUE;
                }
            }

            //If there is no set chance it means it should always happen
            if(statDownChance == 0)
                statDownChance = 100;

            if(statDown && (Random() % 100) < statDownChance){
                if(gBattleMoves[move].split != SPLIT_STATUS){
                    if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                    && gBattleMons[gBattlerTarget].hp != 0
                    && !gProtectStructs[gBattlerTarget].confusionSelfDmg
                    && gBattleMons[gBattlerTarget].statStages[statDownId] != 0)
                    {
                        gBattlerAttacker = gBattlerTarget;
                        PREPARE_STAT_BUFFER(gBattleTextBuff1, statDownId);
                        PREPARE_STRING_BUFFER(gBattleTextBuff2, STRINGID_STATFELL);
                        BattleScriptPushCursor();
                        gBattleMons[gBattlerTarget].statStages[statDownId]--;
                        gBattleScripting.animArg1 = 14 - statDownId;
                        gBattleScripting.animArg2 = 0;
                        SET_STATCHANGER(statDownId, -1, FALSE);
                        BattleScriptPushCursorAndCallback(BattleScript_BattlerAttackStatLowerOnHit);
                        effect++;
                    }
                }
                else if(gBattleMoves[move].split == SPLIT_STATUS){
                    if (gBattleMons[gBattlerTarget].statStages[statDownId] != 0)
					{
                        gBattlerAttacker = gBattlerTarget;
                        PREPARE_STAT_BUFFER(gBattleTextBuff1, statDownId);
                        PREPARE_STRING_BUFFER(gBattleTextBuff2, STRINGID_STATFELL);
                        BattleScriptPushCursor();
                        gBattleMons[gBattlerTarget].statStages[statDownId]--;
                        gBattleScripting.animArg1 = 14 - statDownId;
                        gBattleScripting.animArg2 = 0;
                        SET_STATCHANGER(statDownId, -1, FALSE);
                        BattleScriptPushCursorAndCallback(BattleScript_BattlerAttackStatLowerOnHit);
                        effect++;
					}
                }
            }

            //Target Stat Down
            if(gSignatureMoveList[speciesId].move == move){
                if(gSignatureMoveList[speciesId].modification == SIGNATURE_MOD_SECONDARY_EFFECT){
                    setStatusId = gSignatureMoveList[speciesId].variable;
                    setStatusChance = gSignatureMoveList[speciesId].chance;
                    setStatusargument = gSignatureMoveList[speciesId].argument;
                    setStatus = TRUE;
                }
                else if (gSignatureMoveList[speciesId].modification2 == SIGNATURE_MOD_SECONDARY_EFFECT){
                    setStatusId = gSignatureMoveList[speciesId].variable2;
                    setStatusChance = gSignatureMoveList[speciesId].chance2;
                    setStatusargument = gSignatureMoveList[speciesId].argument2;
                    setStatus = TRUE;
                }
                else if (gSignatureMoveList[speciesId].modification3 == SIGNATURE_MOD_SECONDARY_EFFECT){
                    setStatusId = gSignatureMoveList[speciesId].variable3;
                    setStatusChance = gSignatureMoveList[speciesId].chance3;
                    setStatusargument = gSignatureMoveList[speciesId].argument3;
                    setStatus = TRUE;
                }
                else if (gSignatureMoveList[speciesId].modification4 == SIGNATURE_MOD_SECONDARY_EFFECT){
                    setStatusId = gSignatureMoveList[speciesId].variable4;
                    setStatusChance = gSignatureMoveList[speciesId].chance4;
                    setStatusargument = gSignatureMoveList[speciesId].argument4;
                    setStatus = TRUE;
                }
                else if (gSignatureMoveList[speciesId].modification5 == SIGNATURE_MOD_SECONDARY_EFFECT){
                    setStatusId = gSignatureMoveList[speciesId].variable5;
                    setStatusChance = gSignatureMoveList[speciesId].chance5;
                    setStatusargument = gSignatureMoveList[speciesId].argument5;
                    setStatus = TRUE;
                }
                else if (gSignatureMoveList[speciesId].modification6 == SIGNATURE_MOD_SECONDARY_EFFECT){
                    setStatusId = gSignatureMoveList[speciesId].variable6;
                    setStatusChance = gSignatureMoveList[speciesId].chance6;
                    setStatusargument = gSignatureMoveList[speciesId].argument6;
                    setStatus = TRUE;
                }
            }

            //If there is no set chance it means it should always happen
            if(setStatusChance == 0)
                setStatusChance = 100;

            if(setStatus && (Random() % 100) < setStatusChance){
                switch(setStatusId){
                    case SIGNATURE_SECONDARY_EFFECT_POISON:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && IsBattlerAlive(gBattlerTarget)
                        && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                        && CanBePoisoned(gBattlerAttacker, gBattlerTarget))
                        {
                            gBattleScripting.moveEffect = MOVE_EFFECT_POISON;
                            gLastUsedAbility = ABILITY_SIGNATURE_MOVE;
                            PREPARE_ABILITY_BUFFER(gBattleTextBuff1, gLastUsedAbility);
                            BattleScriptPushCursor();
                            gBattlescriptCurrInstr = BattleScript_AttackerMoveSetsStatusEffect;
                            gHitMarker |= HITMARKER_IGNORE_SAFEGUARD;
                            effect++;
                        }
                    break;
                    case SIGNATURE_SECONDARY_EFFECT_TOXIC:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && IsBattlerAlive(gBattlerTarget)
                        && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                        && CanBePoisoned(gBattlerAttacker, gBattlerTarget))
                        {
                            gBattleScripting.moveEffect = MOVE_EFFECT_TOXIC;
                            gLastUsedAbility = ABILITY_SIGNATURE_MOVE;
                            PREPARE_ABILITY_BUFFER(gBattleTextBuff1, gLastUsedAbility);
                            BattleScriptPushCursor();
                            gBattlescriptCurrInstr = BattleScript_AttackerMoveSetsStatusEffect;
                            gHitMarker |= HITMARKER_IGNORE_SAFEGUARD;
                            effect++;
                        }
                    break;
                    case SIGNATURE_SECONDARY_EFFECT_PARALYSIS:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && IsBattlerAlive(gBattlerTarget)
                        && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                        && CanBeParalyzed(gBattlerAttacker, gBattlerTarget))
                        {
                            gBattleScripting.moveEffect = MOVE_EFFECT_PARALYSIS;
                            gLastUsedAbility = ABILITY_SIGNATURE_MOVE;
                            PREPARE_ABILITY_BUFFER(gBattleTextBuff1, gLastUsedAbility);
                            BattleScriptPushCursor();
                            gBattlescriptCurrInstr = BattleScript_AttackerMoveSetsStatusEffect;
                            gHitMarker |= HITMARKER_IGNORE_SAFEGUARD;
                            effect++;
                        }
                    break;
                    case SIGNATURE_SECONDARY_EFFECT_BURN:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && IsBattlerAlive(gBattlerTarget)
                        && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                        && CanBeBurned(gBattlerTarget))
                        {
                            gBattleScripting.moveEffect = MOVE_EFFECT_BURN;
                            gLastUsedAbility = ABILITY_SIGNATURE_MOVE;
                            PREPARE_ABILITY_BUFFER(gBattleTextBuff1, gLastUsedAbility);
                            BattleScriptPushCursor();
                            gBattlescriptCurrInstr = BattleScript_AttackerMoveSetsStatusEffect;
                            gHitMarker |= HITMARKER_IGNORE_SAFEGUARD;
                            effect++;
                        }
                    break;
                    case SIGNATURE_SECONDARY_EFFECT_SLEEP:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && IsBattlerAlive(gBattlerTarget)
                        && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                        && CanSleep(gBattlerTarget))
                        {
                            gBattleScripting.moveEffect = MOVE_EFFECT_SLEEP;
                            gLastUsedAbility = ABILITY_SIGNATURE_MOVE;
                            PREPARE_ABILITY_BUFFER(gBattleTextBuff1, gLastUsedAbility);
                            BattleScriptPushCursor();
                            gBattlescriptCurrInstr = BattleScript_AttackerMoveSetsStatusEffect;
                            gHitMarker |= HITMARKER_IGNORE_SAFEGUARD;
                            effect++;
                        }
                    break;
                    case SIGNATURE_SECONDARY_EFFECT_FREEZE:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && IsBattlerAlive(gBattlerTarget)
                        && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                        && CanBeFrozen(gBattlerTarget))
                        {
                            gBattleScripting.moveEffect = MOVE_EFFECT_FREEZE;
                            gLastUsedAbility = ABILITY_SIGNATURE_MOVE;
                            PREPARE_ABILITY_BUFFER(gBattleTextBuff1, gLastUsedAbility);
                            BattleScriptPushCursor();
                            gBattlescriptCurrInstr = BattleScript_AttackerMoveSetsStatusEffect;
                            gHitMarker |= HITMARKER_IGNORE_SAFEGUARD;
                            effect++;
                        }
                    break;
                    case SIGNATURE_SECONDARY_EFFECT_CONFUSION:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && IsBattlerAlive(gBattlerTarget)
                        && !gProtectStructs[gBattlerAttacker].confusionSelfDmg
                        && CanBeConfused(gBattlerTarget))
                        {
                            gBattleScripting.moveEffect = MOVE_EFFECT_CONFUSION;
                            PREPARE_ABILITY_BUFFER(gBattleTextBuff1, gLastUsedAbility);
                            BattleScriptPushCursor();
                            BattleScriptPushCursorAndCallback(BattleScript_BattlerAttackConfusesOnHit);
                            effect++;
                        }
                    break;
                    case SIGNATURE_SECONDARY_EFFECT_INFATUATION:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && IsBattlerAlive(gBattlerTarget)
                        && !gProtectStructs[gBattlerTarget].confusionSelfDmg
                        && GetBattlerAbility(gBattlerTarget) != ABILITY_OBLIVIOUS
                        && !IsAbilityOnSide(gBattlerTarget, ABILITY_AROMA_VEIL)
                        && GetGenderFromSpeciesAndPersonality(speciesAtk, pidAtk) != GetGenderFromSpeciesAndPersonality(speciesDef, pidDef)
                        && !(gBattleMons[gBattlerTarget].status2 & STATUS2_INFATUATION)
                        && GetGenderFromSpeciesAndPersonality(speciesAtk, pidAtk) != MON_GENDERLESS
                        && GetGenderFromSpeciesAndPersonality(speciesDef, pidDef) != MON_GENDERLESS)
                        {
                            gBattleMons[gBattlerTarget].status2 |= STATUS2_INFATUATED_WITH(gBattlerAttacker);
                            BattleScriptPushCursor();
                            gBattlescriptCurrInstr = BattleScript_BattlerAttackInfauationOnHit;
                            effect++;
                        }
                    break;
                    case SIGNATURE_SECONDARY_EFFECT_FLINCH:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && IsBattlerAlive(gBattlerTarget)
                        && !gProtectStructs[gBattlerTarget].confusionSelfDmg
                        && GetBattlerAbility(gBattlerTarget) != ABILITY_STEADFAST
                        && !IS_BATTLER_OF_TYPE(gBattlerTarget, TYPE_FIGHTING)
                        && !(gBattleMons[gBattlerTarget].status2 & STATUS2_FLINCHED)
                        && gBattlerAttacker != gBattlerTarget)
                        {
                            gBattleMons[gBattlerTarget].status2 |= STATUS2_FLINCHED;
                        }
                    break;
                    case SIGNATURE_SECONDARY_EFFECT_CURSE:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && IsBattlerAlive(gBattlerTarget)
                        && !gProtectStructs[gBattlerTarget].confusionSelfDmg
                        && !IS_BATTLER_OF_TYPE(gBattlerTarget, TYPE_GHOST)
                        && !(gBattleMons[gBattlerTarget].status2 & STATUS2_CURSED)
                        && gBattlerAttacker != gBattlerTarget)
                        {
                            gBattleMons[gBattlerTarget].status2 |= STATUS2_CURSED;
                            
                            BattleScriptPushCursor();
                            gBattlescriptCurrInstr = BattleScript_BattlerAttackSeededOnHit;
                            effect++;
                        }
                    break;
                    case SIGNATURE_SECONDARY_EFFECT_LEECH_SEED:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && IsBattlerAlive(gBattlerTarget)
                        && !gProtectStructs[gBattlerTarget].confusionSelfDmg
                        && GetBattlerAbility(gBattlerTarget) != ABILITY_SAP_SIPPER
                        && !IS_BATTLER_OF_TYPE(gBattlerTarget, TYPE_GRASS)
                        && gBattlerAttacker != gBattlerTarget)
                        {
                            gStatuses3[gActiveBattler] |= STATUS3_LEECHSEED_BATTLER;
                            gStatuses3[gBattlerTarget] |= STATUS3_LEECHSEED;

                            BattleScriptPushCursor();
                            gBattlescriptCurrInstr = BattleScript_BattlerAttackSeededOnHit;
                            effect++;
                        }
                    break;
                    case SIGNATURE_SECONDARY_EFFECT_REMOVE_STAT_CHANGES:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && IsBattlerAlive(gBattlerTarget)
                        && TryResetBattlerStatChanges(gBattlerTarget)
                        && gBattlerAttacker != gBattlerTarget)
                        {
                            BattleScriptPushCursor();
                            gBattlescriptCurrInstr = BattleScript_TargetStatsWereAllReset;
                            effect++;
                        }
                    break;
                    case SIGNATURE_SECONDARY_EFFECT_TAUNT:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && IsBattlerAlive(gBattlerTarget)
                        && !IsAbilityOnSide(gBattlerTarget, ABILITY_AROMA_VEIL)
                        && !GetBattlerAbility(gBattlerTarget) != ABILITY_OBLIVIOUS
                        && gDisableStructs[gBattlerTarget].tauntTimer == 0
                        && gBattlerAttacker != gBattlerTarget)
                        {
                            gDisableStructs[gBattlerTarget].tauntTimer = 2;
                            BattleScriptPushCursor();
                            gBattlescriptCurrInstr = BattleScript_TargetWasTaunted;
                            effect++;
                        }
                    break;
                    case SIGNATURE_SECONDARY_EFFECT_GIVE_THIRD_TYPE:
                        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                        && IsBattlerAlive(gBattlerTarget)
                        && !IS_BATTLER_OF_TYPE(gBattlerTarget, setStatusargument)
                        && gDisableStructs[gBattlerTarget].tauntTimer == 0
                        && gBattlerAttacker != gBattlerTarget)
                        {
                            gBattleMons[gBattlerTarget].type3 = setStatusargument;
				            PREPARE_TYPE_BUFFER(gBattleTextBuff1, gBattleMons[gBattlerTarget].type3);
                            BattleScriptPushCursor();
                            gBattlescriptCurrInstr = BattleScript_BattlerBecameXType;
                            effect++;
                        }
                    break;
                }
            }
        }

    case ABILITYEFFECT_MOVE_END_OTHER: // Abilities that activate on *another* battler's moveend: Dancer, Soul-Heart, Receiver, Symbiosis
        switch (GetBattlerAbility(battler))
        {
        case ABILITY_DANCER:
            if (IsBattlerAlive(battler)
             && (gBattleMoves[gCurrentMove].flags & FLAG_DANCE)
             && !gSpecialStatuses[battler].dancerUsedMove
             && gBattlerAttacker != battler)
            {
                // Set bit and save Dancer mon's original target
                gSpecialStatuses[battler].dancerUsedMove = 1;
                gSpecialStatuses[battler].dancerOriginalTarget = *(gBattleStruct->moveTarget + battler) | 0x4;
                gBattleStruct->atkCancellerTracker = 0;
                gBattlerAttacker = gBattlerAbility = battler;
                gCalledMove = gCurrentMove;

                // Set the target to the original target of the mon that first used a Dance move
                gBattlerTarget = gBattleScripting.savedBattler & 0x3;

                // Make sure that the target isn't an ally - if it is, target the original user
                if (GetBattlerSide(gBattlerTarget) == GetBattlerSide(gBattlerAttacker))
                    gBattlerTarget = (gBattleScripting.savedBattler & 0xF0) >> 4;
                gHitMarker &= ~(HITMARKER_ATTACKSTRING_PRINTED);
                BattleScriptExecute(BattleScript_DancerActivates);
                effect++;
            }
            break;
        }
        break;
    case ABILITYEFFECT_IMMUNITY: // 5
        for (battler = 0; battler < gBattlersCount; battler++)
        {
            switch (gBattleMons[battler].ability)
            {
            case ABILITY_IMMUNITY:
                if (gBattleMons[battler].status1 & (STATUS1_POISON | STATUS1_TOXIC_POISON | STATUS1_TOXIC_COUNTER))
                {
                    StringCopy(gBattleTextBuff1, gStatusConditionString_PoisonJpn);
                    effect = 1;
                }
                break;
            case ABILITY_OWN_TEMPO:
                if (gBattleMons[battler].status2 & STATUS2_CONFUSION)
                {
                    StringCopy(gBattleTextBuff1, gStatusConditionString_ConfusionJpn);
                    effect = 2;
                }
                break;
            case ABILITY_LIMBER:
                if (gBattleMons[battler].status1 & STATUS1_PARALYSIS)
                {
                    StringCopy(gBattleTextBuff1, gStatusConditionString_ParalysisJpn);
                    effect = 1;
                }
                break;
            case ABILITY_INSOMNIA:
            case ABILITY_VITAL_SPIRIT:
                if (gBattleMons[battler].status1 & STATUS1_SLEEP)
                {
                    gBattleMons[battler].status2 &= ~(STATUS2_NIGHTMARE);
                    StringCopy(gBattleTextBuff1, gStatusConditionString_SleepJpn);
                    effect = 1;
                }
                break;
            case ABILITY_WATER_VEIL:
                if (gBattleMons[battler].status1 & STATUS1_BURN)
                {
                    StringCopy(gBattleTextBuff1, gStatusConditionString_BurnJpn);
                    effect = 1;
                }
                break;
            case ABILITY_MAGMA_ARMOR:
                if (gBattleMons[battler].status1 & STATUS1_FREEZE)
                {
                    StringCopy(gBattleTextBuff1, gStatusConditionString_IceJpn);
                    effect = 1;
                }
                break;
            case ABILITY_OBLIVIOUS:
                if (gBattleMons[battler].status2 & STATUS2_INFATUATION)
                {
                    StringCopy(gBattleTextBuff1, gStatusConditionString_LoveJpn);
                    effect = 3;
                }
                break;
            }
            if (effect)
            {
                switch (effect)
                {
                case 1: // status cleared
                    gBattleMons[battler].status1 = 0;
                    break;
                case 2: // get rid of confusion
                    gBattleMons[battler].status2 &= ~(STATUS2_CONFUSION);
                    break;
                case 3: // get rid of infatuation
                    gBattleMons[battler].status2 &= ~(STATUS2_INFATUATION);
                    break;
                }

                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_AbilityCuredStatus;
                gBattleScripting.battler = gActiveBattler = gBattlerAbility = battler;
                BtlController_EmitSetMonData(0, REQUEST_STATUS_BATTLE, 0, 4, &gBattleMons[gActiveBattler].status1);
                MarkBattlerForControllerExec(gActiveBattler);
                return effect;
            }
        }
        break;
    /*/case ABILITYEFFECT_FORECAST: // 6
        for (battler = 0; battler < gBattlersCount; battler++)
        {
            if (gBattleMons[battler].ability == ABILITY_FORECAST || gBattleMons[battler].ability == ABILITY_FLOWER_GIFT)
            {
                effect = TryWeatherFormChange(battler);
                if (effect)
                {
                    BattleScriptPushCursorAndCallback(BattleScript_CastformChange);
                    gBattleScripting.battler = battler;
                    gBattleStruct->formToChangeInto = effect - 1;
                    return effect;
                }
            }
        }
        break;/*/
    case ABILITYEFFECT_SYNCHRONIZE:
        if (gLastUsedAbility == ABILITY_SYNCHRONIZE && (gHitMarker & HITMARKER_SYNCHRONISE_EFFECT))
        {
            gHitMarker &= ~(HITMARKER_SYNCHRONISE_EFFECT);

            if (!(gBattleMons[gBattlerAttacker].status1 & STATUS1_ANY))
            {
                gBattleStruct->synchronizeMoveEffect &= ~(MOVE_EFFECT_AFFECTS_USER | MOVE_EFFECT_CERTAIN);
                if (gBattleStruct->synchronizeMoveEffect == MOVE_EFFECT_TOXIC)
                    gBattleStruct->synchronizeMoveEffect = MOVE_EFFECT_POISON;

                gBattleScripting.moveEffect = gBattleStruct->synchronizeMoveEffect + MOVE_EFFECT_AFFECTS_USER;
                gBattleScripting.battler = gBattlerAbility = gBattlerTarget;
                PREPARE_ABILITY_BUFFER(gBattleTextBuff1, ABILITY_SYNCHRONIZE);
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_SynchronizeActivates;
                gHitMarker |= HITMARKER_IGNORE_SAFEGUARD;
                effect++;
            }
        }
        break;
    case ABILITYEFFECT_ATK_SYNCHRONIZE: // 8
        if (gLastUsedAbility == ABILITY_SYNCHRONIZE && (gHitMarker & HITMARKER_SYNCHRONISE_EFFECT))
        {
            gHitMarker &= ~(HITMARKER_SYNCHRONISE_EFFECT);

            if (!(gBattleMons[gBattlerTarget].status1 & STATUS1_ANY))
            {
                gBattleStruct->synchronizeMoveEffect &= ~(MOVE_EFFECT_AFFECTS_USER | MOVE_EFFECT_CERTAIN);
                if (gBattleStruct->synchronizeMoveEffect == MOVE_EFFECT_TOXIC)
                    gBattleStruct->synchronizeMoveEffect = MOVE_EFFECT_POISON;

                gBattleScripting.moveEffect = gBattleStruct->synchronizeMoveEffect;
                gBattleScripting.battler = gBattlerAbility = gBattlerAttacker;
                PREPARE_ABILITY_BUFFER(gBattleTextBuff1, ABILITY_SYNCHRONIZE);
                BattleScriptPushCursor();
                gBattlescriptCurrInstr = BattleScript_SynchronizeActivates;
                gHitMarker |= HITMARKER_IGNORE_SAFEGUARD;
                effect++;
            }
        }
        break;
	case ABILITYEFFECT_INTIMIDATE1:
    case ABILITYEFFECT_INTIMIDATE2:
        for (i = 0; i < gBattlersCount; i++)
        {
            if (gBattleMons[i].ability == ABILITY_INTIMIDATE && gBattleResources->flags->flags[i] & RESOURCE_FLAG_INTIMIDATED)
            {
                gLastUsedAbility = ABILITY_INTIMIDATE;
                gBattleResources->flags->flags[i] &= ~(RESOURCE_FLAG_INTIMIDATED);
                if (caseID == ABILITYEFFECT_INTIMIDATE1)
                {
                    BattleScriptPushCursorAndCallback(BattleScript_IntimidateActivatesEnd3);
                }
                else
                {
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_IntimidateActivates;
                }
                battler = gBattlerAbility = gBattleStruct->intimidateBattler = i;
                effect++;
                break;
            }
        }
        break;
    case ABILITYEFFECT_TRACE1:
    case ABILITYEFFECT_TRACE2:
        for (i = 0; i < gBattlersCount; i++)
        {
            if (gBattleMons[i].ability == ABILITY_TRACE && (gBattleResources->flags->flags[i] & RESOURCE_FLAG_TRACED))
            {
                u8 side = (GetBattlerPosition(i) ^ BIT_SIDE) & BIT_SIDE; // side of the opposing pokemon
                u8 target1 = GetBattlerAtPosition(side);
                u8 target2 = GetBattlerAtPosition(side + BIT_FLANK);

                if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
                {
                    if (!sAbilitiesNotTraced[gBattleMons[target1].ability] && gBattleMons[target1].hp != 0
                     && !sAbilitiesNotTraced[gBattleMons[target2].ability] && gBattleMons[target2].hp != 0)
                        gActiveBattler = GetBattlerAtPosition(((Random() & 1) * 2) | side), effect++;
                    else if (!sAbilitiesNotTraced[gBattleMons[target1].ability] && gBattleMons[target1].hp != 0)
                        gActiveBattler = target1, effect++;
                    else if (!sAbilitiesNotTraced[gBattleMons[target2].ability] && gBattleMons[target2].hp != 0)
                        gActiveBattler = target2, effect++;
                }
                else
                {
                    if (!sAbilitiesNotTraced[gBattleMons[target1].ability] && gBattleMons[target1].hp != 0)
                        gActiveBattler = target1, effect++;
                }

                if (effect)
                {
                    if (caseID == ABILITYEFFECT_TRACE1)
                    {
                        BattleScriptPushCursorAndCallback(BattleScript_TraceActivatesEnd3);
                    }
                    else
                    {
                        BattleScriptPushCursor();
                        gBattlescriptCurrInstr = BattleScript_TraceActivates;
                    }
                    gBattleResources->flags->flags[i] &= ~(RESOURCE_FLAG_TRACED);
                    gBattleStruct->tracedAbility[i] = gLastUsedAbility = gBattleMons[gActiveBattler].ability;
                    battler = gBattlerAbility = gBattleScripting.battler = i;

                    PREPARE_MON_NICK_WITH_PREFIX_BUFFER(gBattleTextBuff1, gActiveBattler, gBattlerPartyIndexes[gActiveBattler])
                    PREPARE_ABILITY_BUFFER(gBattleTextBuff2, gLastUsedAbility)
                    break;
                }
            }
        }
        break;
    }

    if (effect && gLastUsedAbility != 0xFF)
        RecordAbilityBattle(battler, gLastUsedAbility);
    if (effect && caseID <= ABILITYEFFECT_MOVE_END)
        gBattlerAbility = battler;

    return effect;
}

u32 GetBattlerAbility(u8 battlerId)
{
    if (gStatuses3[battlerId] & STATUS3_GASTRO_ACID)
        return ABILITY_NONE;
    else if ((((gBattleMons[gBattlerAttacker].ability == ABILITY_MOLD_BREAKER
            || gBattleMons[gBattlerAttacker].ability == ABILITY_TERAVOLT
            || gBattleMons[gBattlerAttacker].ability == ABILITY_TURBOBLAZE)
            && !(gStatuses3[gBattlerAttacker] & STATUS3_GASTRO_ACID))
            || gBattleMoves[gCurrentMove].flags & FLAG_TARGET_ABILITY_IGNORED)
            && sAbilitiesAffectedByMoldBreaker[gBattleMons[battlerId].ability]
            && gBattlerByTurnOrder[gCurrentTurnActionNumber] == gBattlerAttacker
            && gActionsByTurnOrder[gBattlerByTurnOrder[gBattlerAttacker]] == B_ACTION_USE_MOVE
            && gCurrentTurnActionNumber < gBattlersCount)
        return ABILITY_NONE;
    else
        return gBattleMons[battlerId].ability;
}

u32 IsAbilityOnSide(u32 battlerId, u32 ability)
{
    if (IsBattlerAlive(battlerId) && GetBattlerAbility(battlerId) == ability)
        return battlerId + 1;
    else if (IsBattlerAlive(BATTLE_PARTNER(battlerId)) && GetBattlerAbility(BATTLE_PARTNER(battlerId)) == ability)
        return BATTLE_PARTNER(battlerId) + 1;
    else
        return 0;
}

u32 IsAbilityOnOpposingSide(u32 battlerId, u32 ability)
{
    return IsAbilityOnSide(BATTLE_OPPOSITE(battlerId), ability);
}

u32 IsAbilityOnField(u32 ability)
{
    u32 i;

    for (i = 0; i < gBattlersCount; i++)
    {
        if (IsBattlerAlive(i) && GetBattlerAbility(i) == ability)
            return i + 1;
    }

    return 0;
}

u32 IsAbilityOnFieldExcept(u32 battlerId, u32 ability)
{
    u32 i;

    for (i = 0; i < gBattlersCount; i++)
    {
        if (i != battlerId && IsBattlerAlive(i) && GetBattlerAbility(i) == ability)
            return i + 1;
    }

    return 0;
}

u32 IsAbilityPreventingEscape(u32 battlerId)
{
    u32 id;

    if (B_GHOSTS_ESCAPE >= GEN_6 && IS_BATTLER_OF_TYPE(battlerId, TYPE_GHOST))
        return 0;

    if ((id = IsAbilityOnOpposingSide(battlerId, ABILITY_SHADOW_TAG)) && gBattleMons[battlerId].ability != ABILITY_SHADOW_TAG)
        return id;
    if ((id = IsAbilityOnOpposingSide(battlerId, ABILITY_ARENA_TRAP)) && IsBattlerGrounded(battlerId))
        return id;
    if ((id = IsAbilityOnOpposingSide(battlerId, ABILITY_MAGNET_PULL)) && IS_BATTLER_OF_TYPE(battlerId, TYPE_STEEL))
        return id;

    return 0;
}

bool32 CanBattlerEscape(u32 battlerId) // no ability check
{
    return (GetBattlerHoldEffect(battlerId, TRUE) == HOLD_EFFECT_SHED_SHELL
            || !((gBattleMons[battlerId].status2 & (STATUS2_ESCAPE_PREVENTION | STATUS2_WRAPPED))
                || (gStatuses3[battlerId] & STATUS3_ROOTED)
                || gFieldStatuses & STATUS_FIELD_FAIRY_LOCK));
}

void BattleScriptExecute(const u8 *BS_ptr)
{
    gBattlescriptCurrInstr = BS_ptr;
    gBattleResources->battleCallbackStack->function[gBattleResources->battleCallbackStack->size++] = gBattleMainFunc;
    gBattleMainFunc = RunBattleScriptCommands_PopCallbacksStack;
    gCurrentActionFuncId = 0;
}

void BattleScriptPushCursorAndCallback(const u8 *BS_ptr)
{
    BattleScriptPushCursor();
    gBattlescriptCurrInstr = BS_ptr;
    gBattleResources->battleCallbackStack->function[gBattleResources->battleCallbackStack->size++] = gBattleMainFunc;
    gBattleMainFunc = RunBattleScriptCommands;
}

enum
{
    ITEM_NO_EFFECT, // 0
    ITEM_STATUS_CHANGE, // 1
    ITEM_EFFECT_OTHER, // 2
    ITEM_PP_CHANGE, // 3
    ITEM_HP_CHANGE, // 4
    ITEM_STATS_CHANGE, // 5
};

// second argument is 1/X of current hp compared to max hp
static bool32 HasEnoughHpToEatBerry(u32 battlerId, u32 hpFraction, u32 itemId)
{
    bool32 isBerry = (ItemId_GetPocket(itemId) == POCKET_BERRIES);

    if (gBattleMons[battlerId].hp == 0)
        return FALSE;
    // Unnerve prevents consumption of opponents' berries.
    if (isBerry && IsAbilityOnOpposingSide(battlerId, ABILITY_UNNERVE))
        return FALSE;
    if (gBattleMons[battlerId].hp <= gBattleMons[battlerId].maxHP / hpFraction)
        return TRUE;

    if (hpFraction <= 4 && GetBattlerAbility(battlerId) == ABILITY_GLUTTONY && isBerry
         && gBattleMons[battlerId].hp <= gBattleMons[battlerId].maxHP / 2)
    {
        RecordAbilityBattle(battlerId, ABILITY_GLUTTONY);
        return TRUE;
    }

    return FALSE;
}

static u8 HealConfuseBerry(u32 battlerId, u32 itemId, u8 flavorId)
{
    if (HasEnoughHpToEatBerry(battlerId, 2, itemId))
    {
        PREPARE_FLAVOR_BUFFER(gBattleTextBuff1, flavorId);

        gBattleMoveDamage = gBattleMons[battlerId].maxHP / GetBattlerHoldEffectParam(battlerId);
        if (gBattleMoveDamage == 0)
            gBattleMoveDamage = 1;
        gBattleMoveDamage *= -1;
        if (GetFlavorRelationByPersonality(gBattleMons[battlerId].personality, flavorId) < 0)
            BattleScriptExecute(BattleScript_BerryConfuseHealEnd2);
        else
            BattleScriptExecute(BattleScript_ItemHealHP_RemoveItemEnd2);

        return ITEM_HP_CHANGE;
    }
    return 0;
}

static u8 StatRaiseBerry(u32 battlerId, u32 itemId, u32 statId)
{
    if (gBattleMons[battlerId].statStages[statId] < 0xC && HasEnoughHpToEatBerry(battlerId, GetBattlerHoldEffectParam(battlerId), itemId))
    {
        PREPARE_STAT_BUFFER(gBattleTextBuff1, statId);
        PREPARE_STRING_BUFFER(gBattleTextBuff2, STRINGID_STATROSE);

        gEffectBattler = battlerId;
        SET_STATCHANGER(statId, 1, FALSE);
        gBattleScripting.animArg1 = 0xE + statId;
        gBattleScripting.animArg2 = 0;
        BattleScriptExecute(BattleScript_BerryStatRaiseEnd2);
        return ITEM_STATS_CHANGE;
    }
    return 0;
}

static u8 RandomStatRaiseBerry(u32 battlerId, u32 itemId)
{
    s32 i;

    for (i = 0; i < 5; i++)
    {
        if (gBattleMons[battlerId].statStages[STAT_ATK + i] < 0xC)
            break;
    }
    if (i != 5 && HasEnoughHpToEatBerry(battlerId, GetBattlerHoldEffectParam(battlerId), itemId))
    {
        do
        {
            i = Random() % 5;
        } while (gBattleMons[battlerId].statStages[STAT_ATK + i] == 0xC);

        PREPARE_STAT_BUFFER(gBattleTextBuff1, i + 1);

        gBattleTextBuff2[0] = B_BUFF_PLACEHOLDER_BEGIN;
        gBattleTextBuff2[1] = B_BUFF_STRING;
        gBattleTextBuff2[2] = STRINGID_STATSHARPLY;
        gBattleTextBuff2[3] = STRINGID_STATSHARPLY >> 8;
        gBattleTextBuff2[4] = B_BUFF_STRING;
        gBattleTextBuff2[5] = STRINGID_STATROSE;
        gBattleTextBuff2[6] = STRINGID_STATROSE >> 8;
        gBattleTextBuff2[7] = EOS;

        gEffectBattler = battlerId;
        SET_STATCHANGER(i + 1, 2, FALSE);
        gBattleScripting.animArg1 = 0x21 + i + 6;
        gBattleScripting.animArg2 = 0;
        BattleScriptExecute(BattleScript_BerryStatRaiseEnd2);
        return ITEM_STATS_CHANGE;
    }
    return 0;
}

static u8 ItemHealHp(u32 battlerId, u32 itemId, bool32 end2, bool32 percentHeal)
{
    if (HasEnoughHpToEatBerry(battlerId, 2, itemId))
    {
        if (percentHeal)
            gBattleMoveDamage = (gBattleMons[battlerId].maxHP * GetBattlerHoldEffectParam(battlerId) / 100) * -1;
        else
            gBattleMoveDamage = GetBattlerHoldEffectParam(battlerId) * -1;

        if (end2)
        {
            BattleScriptExecute(BattleScript_ItemHealHP_RemoveItemEnd2);
        }
        else
        {
            BattleScriptPushCursor();
            gBattlescriptCurrInstr = BattleScript_ItemHealHP_RemoveItemRet;
        }
        return ITEM_HP_CHANGE;
    }
    return 0;
}

static bool32 UnnerveOn(u32 battlerId, u32 itemId)
{
    if (ItemId_GetPocket(itemId) == POCKET_BERRIES && IsAbilityOnOpposingSide(battlerId, ABILITY_UNNERVE))
        return TRUE;
    return FALSE;
}

u8 ItemBattleEffects(u8 caseID, u8 battlerId, bool8 moveTurn)
{
    int i = 0, moveType;
    u8 effect = ITEM_NO_EFFECT;
    u8 changedPP = 0;
    u8 battlerHoldEffect, atkHoldEffect;
    u8 atkHoldEffectParam;
    u16 atkItem;

    gLastUsedItem = gBattleMons[battlerId].item;
    battlerHoldEffect = GetBattlerHoldEffect(battlerId, TRUE);

    atkItem = gBattleMons[gBattlerAttacker].item;
    atkHoldEffect = GetBattlerHoldEffect(gBattlerAttacker, TRUE);
    atkHoldEffectParam = GetBattlerHoldEffectParam(gBattlerAttacker);

    switch (caseID)
    {
    case ITEMEFFECT_ON_SWITCH_IN:
        if (!gSpecialStatuses[battlerId].switchInItemDone)
        {
            switch (battlerHoldEffect)
            {
            case HOLD_EFFECT_DOUBLE_PRIZE:
                if (GetBattlerSide(battlerId) == B_SIDE_PLAYER && gBattleStruct->moneyMultiplier < 2)
                    gBattleStruct->moneyMultiplier *= 2;
                break;
            case HOLD_EFFECT_RESTORE_STATS:
                for (i = 0; i < NUM_BATTLE_STATS; i++)
                {
                    if (gBattleMons[battlerId].statStages[i] < DEFAULT_STAT_STAGE)
                    {
                        gBattleMons[battlerId].statStages[i] = DEFAULT_STAT_STAGE;
                        effect = ITEM_STATS_CHANGE;
                    }
                }
                if (effect)
                {
                    gBattleScripting.battler = battlerId;
                    gPotentialItemEffectBattler = battlerId;
                    gActiveBattler = gBattlerAttacker = battlerId;
                    BattleScriptExecute(BattleScript_WhiteHerbEnd2);
                }
                break;
            case HOLD_EFFECT_CONFUSE_SPICY:
                if (B_BERRIES_INSTANT >= GEN_4)
                    effect = HealConfuseBerry(battlerId, gLastUsedItem, FLAVOR_SPICY);
                break;
            case HOLD_EFFECT_CONFUSE_DRY:
                if (B_BERRIES_INSTANT >= GEN_4)
                    effect = HealConfuseBerry(battlerId, gLastUsedItem, FLAVOR_DRY);
                break;
            case HOLD_EFFECT_CONFUSE_SWEET:
                if (B_BERRIES_INSTANT >= GEN_4)
                    effect = HealConfuseBerry(battlerId, gLastUsedItem, FLAVOR_SWEET);
                break;
            case HOLD_EFFECT_CONFUSE_BITTER:
                if (B_BERRIES_INSTANT >= GEN_4)
                    effect = HealConfuseBerry(battlerId, gLastUsedItem, FLAVOR_BITTER);
                break;
            case HOLD_EFFECT_CONFUSE_SOUR:
                if (B_BERRIES_INSTANT >= GEN_4)
                    effect = HealConfuseBerry(battlerId, gLastUsedItem, FLAVOR_SOUR);
                break;
            case HOLD_EFFECT_ATTACK_UP:
                if (B_BERRIES_INSTANT >= GEN_4)
                    effect = StatRaiseBerry(battlerId, gLastUsedItem, STAT_ATK);
                break;
            case HOLD_EFFECT_DEFENSE_UP:
                if (B_BERRIES_INSTANT >= GEN_4)
                    effect = StatRaiseBerry(battlerId, gLastUsedItem, STAT_DEF);
                break;
            case HOLD_EFFECT_SPEED_UP:
                if (B_BERRIES_INSTANT >= GEN_4)
                    effect = StatRaiseBerry(battlerId, gLastUsedItem, STAT_SPEED);
                break;
            case HOLD_EFFECT_SP_ATTACK_UP:
                if (B_BERRIES_INSTANT >= GEN_4)
                    effect = StatRaiseBerry(battlerId, gLastUsedItem, STAT_SPATK);
                break;
            case HOLD_EFFECT_SP_DEFENSE_UP:
                if (B_BERRIES_INSTANT >= GEN_4)
                    effect = StatRaiseBerry(battlerId, gLastUsedItem, STAT_SPDEF);
                break;
            case HOLD_EFFECT_CRITICAL_UP:
                if (B_BERRIES_INSTANT >= GEN_4 && !(gBattleMons[battlerId].status2 & STATUS2_FOCUS_ENERGY) && HasEnoughHpToEatBerry(battlerId, GetBattlerHoldEffectParam(battlerId), gLastUsedItem))
                {
                    gBattleMons[battlerId].status2 |= STATUS2_FOCUS_ENERGY;
                    BattleScriptExecute(BattleScript_BerryFocusEnergyEnd2);
                    effect = ITEM_EFFECT_OTHER;
                }
                break;
            case HOLD_EFFECT_RANDOM_STAT_UP:
                if (B_BERRIES_INSTANT >= GEN_4)
                    effect = RandomStatRaiseBerry(battlerId, gLastUsedItem);
                break;
            case HOLD_EFFECT_CURE_PAR:
                if (B_BERRIES_INSTANT >= GEN_4 && gBattleMons[battlerId].status1 & STATUS1_PARALYSIS && !UnnerveOn(battlerId, gLastUsedItem))
                {
                    gBattleMons[battlerId].status1 &= ~(STATUS1_PARALYSIS);
                    BattleScriptExecute(BattleScript_BerryCurePrlzEnd2);
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_PSN:
                if (B_BERRIES_INSTANT >= GEN_4 && gBattleMons[battlerId].status1 & STATUS1_PSN_ANY && !UnnerveOn(battlerId, gLastUsedItem))
                {
                    gBattleMons[battlerId].status1 &= ~(STATUS1_PSN_ANY | STATUS1_TOXIC_COUNTER);
                    BattleScriptExecute(BattleScript_BerryCurePsnEnd2);
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_BRN:
                if (B_BERRIES_INSTANT >= GEN_4 && gBattleMons[battlerId].status1 & STATUS1_BURN && !UnnerveOn(battlerId, gLastUsedItem))
                {
                    gBattleMons[battlerId].status1 &= ~(STATUS1_BURN);
                    BattleScriptExecute(BattleScript_BerryCureBrnEnd2);
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_FRZ:
                if (B_BERRIES_INSTANT >= GEN_4 && gBattleMons[battlerId].status1 & STATUS1_FREEZE && !UnnerveOn(battlerId, gLastUsedItem))
                {
                    gBattleMons[battlerId].status1 &= ~(STATUS1_FREEZE);
                    BattleScriptExecute(BattleScript_BerryCureFrzEnd2);
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_SLP:
                if (B_BERRIES_INSTANT >= GEN_4 && gBattleMons[battlerId].status1 & STATUS1_SLEEP && !UnnerveOn(battlerId, gLastUsedItem))
                {
                    gBattleMons[battlerId].status1 &= ~(STATUS1_SLEEP);
                    gBattleMons[battlerId].status2 &= ~(STATUS2_NIGHTMARE);
                    BattleScriptExecute(BattleScript_BerryCureSlpEnd2);
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_STATUS:
                if (B_BERRIES_INSTANT >= GEN_4 && (gBattleMons[battlerId].status1 & STATUS1_ANY || gBattleMons[battlerId].status2 & STATUS2_CONFUSION) && !UnnerveOn(battlerId, gLastUsedItem))
                {
                    i = 0;
                    if (gBattleMons[battlerId].status1 & STATUS1_PSN_ANY)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_PoisonJpn);
                        i++;
                    }
                    if (gBattleMons[battlerId].status1 & STATUS1_SLEEP)
                    {
                        gBattleMons[battlerId].status2 &= ~(STATUS2_NIGHTMARE);
                        StringCopy(gBattleTextBuff1, gStatusConditionString_SleepJpn);
                        i++;
                    }
                    if (gBattleMons[battlerId].status1 & STATUS1_PARALYSIS)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_ParalysisJpn);
                        i++;
                    }
                    if (gBattleMons[battlerId].status1 & STATUS1_BURN)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_BurnJpn);
                        i++;
                    }
                    if (gBattleMons[battlerId].status1 & STATUS1_FREEZE)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_IceJpn);
                        i++;
                    }
                    if (gBattleMons[battlerId].status2 & STATUS2_CONFUSION)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_ConfusionJpn);
                        i++;
                    }
                    if (!(i > 1))
                        gBattleCommunication[MULTISTRING_CHOOSER] = 0;
                    else
                        gBattleCommunication[MULTISTRING_CHOOSER] = 1;
                    gBattleMons[battlerId].status1 = 0;
                    gBattleMons[battlerId].status2 &= ~(STATUS2_CONFUSION);
                    BattleScriptExecute(BattleScript_BerryCureChosenStatusEnd2);
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_RESTORE_HP:
                if (gBattleMons[battlerId].hp <= gBattleMons[battlerId].maxHP / 2 && !moveTurn)
                {
                    /*/gBattleMoveDamage = GetBattlerHoldEffectParam(battlerId);
                    if (gBattleMons[battlerId].hp + GetBattlerHoldEffectParam(battlerId) > gBattleMons[battlerId].maxHP)
                        gBattleMoveDamage = gBattleMons[battlerId].maxHP - gBattleMons[battlerId].hp;
                    gBattleMoveDamage *= -1;
                    BattleScriptExecute(BattleScript_ItemHealHP_RemoveItemRet);
                    effect = 4;/*/
					effect = ItemHealHp(battlerId, gLastUsedItem, TRUE, FALSE);
                }
                break;
            case HOLD_EFFECT_RESTORE_PCT_HP:
                if (gBattleMons[battlerId].hp <= gBattleMons[battlerId].maxHP / 2 && !moveTurn)
                {
                    //gBattleMoveDamage = (gBattleMons[battlerId].maxHP * GetBattlerHoldEffectParam(battlerId)) / 100;
                    //if (gBattleMons[battlerId].hp + GetBattlerHoldEffectParam(battlerId) > gBattleMons[battlerId].maxHP)
                    //    gBattleMoveDamage = gBattleMons[battlerId].maxHP - gBattleMons[battlerId].hp;
                    //gBattleMoveDamage *= -1;
                    //BattleScriptExecute(BattleScript_ItemHealHP_RemoveItemRet);
                    effect = ItemHealHp(battlerId, gLastUsedItem, TRUE, TRUE);
                }
                break;
            case HOLD_EFFECT_AIR_BALLOON:
                effect = ITEM_EFFECT_OTHER;
                gBattleScripting.battler = battlerId;
                BattleScriptPushCursorAndCallback(BattleScript_AirBaloonMsgIn);
                RecordItemEffectBattle(battlerId, HOLD_EFFECT_AIR_BALLOON);
                break;
            }

            if (effect)
            {
                gSpecialStatuses[battlerId].switchInItemDone = 1;
                gActiveBattler = gBattlerAttacker = gPotentialItemEffectBattler = gBattleScripting.battler = battlerId;
                switch (effect)
                {
                case ITEM_STATUS_CHANGE:
                    BtlController_EmitSetMonData(0, REQUEST_STATUS_BATTLE, 0, 4, &gBattleMons[battlerId].status1);
                    MarkBattlerForControllerExec(gActiveBattler);
                    break;
                case ITEM_PP_CHANGE:
                    if (!(gBattleMons[battlerId].status2 & STATUS2_TRANSFORMED) && !(gDisableStructs[battlerId].mimickedMoves & gBitTable[i]))
                        gBattleMons[battlerId].pp[i] = changedPP;
                    break;
                }
            }
        }
        break;
    case 1:
        if (gBattleMons[battlerId].hp)
        {
            switch (battlerHoldEffect)
            {
            case HOLD_EFFECT_RESTORE_HP:
                if (!moveTurn)
                    effect = ItemHealHp(battlerId, gLastUsedItem, TRUE, FALSE);
                break;
            case HOLD_EFFECT_RESTORE_PCT_HP:
                if (!moveTurn)
                    effect = ItemHealHp(battlerId, gLastUsedItem, TRUE, TRUE);//asdf
                break;
            case HOLD_EFFECT_RESTORE_PP:
                if (!moveTurn)
                {
                    struct Pokemon *mon;
                    u8 ppBonuses;
                    u16 move;

                    if (GetBattlerSide(battlerId) == B_SIDE_PLAYER)
                        mon = &gPlayerParty[gBattlerPartyIndexes[battlerId]];
                    else
                        mon = &gEnemyParty[gBattlerPartyIndexes[battlerId]];
                    for (i = 0; i < MAX_MON_MOVES; i++)
                    {
                        move = GetMonData(mon, MON_DATA_MOVE1 + i);
                        changedPP = GetMonData(mon, MON_DATA_PP1 + i);
                        ppBonuses = GetMonData(mon, MON_DATA_PP_BONUSES);
                        if (move && changedPP == 0)
                            break;
                    }
                    if (i != MAX_MON_MOVES)
                    {
                        u8 maxPP = CalculatePPWithBonus(move, ppBonuses, i);
                        if (changedPP + GetBattlerHoldEffectParam(battlerId) > maxPP)
                            changedPP = maxPP;
                        else
                            changedPP = changedPP + GetBattlerHoldEffectParam(battlerId);

                        PREPARE_MOVE_BUFFER(gBattleTextBuff1, move);

                        BattleScriptExecute(BattleScript_BerryPPHealEnd2);
                        BtlController_EmitSetMonData(0, i + REQUEST_PPMOVE1_BATTLE, 0, 1, &changedPP);
                        MarkBattlerForControllerExec(gActiveBattler);
                        effect = ITEM_PP_CHANGE;
                    }
                }
                break;
            case HOLD_EFFECT_RESTORE_STATS:
                for (i = 0; i < NUM_BATTLE_STATS; i++)
                {
                    if (gBattleMons[battlerId].statStages[i] < 6)
                    {
                        gBattleMons[battlerId].statStages[i] = 6;
                        effect = ITEM_STATS_CHANGE;
                    }
                }
                if (effect)
                {
                    gBattleScripting.battler = battlerId;
                    gPotentialItemEffectBattler = battlerId;
                    gActiveBattler = gBattlerAttacker = battlerId;
                    BattleScriptExecute(BattleScript_WhiteHerbEnd2);
                }
                break;
            case HOLD_EFFECT_BLACK_SLUDGE:
                if (IS_BATTLER_OF_TYPE(battlerId, TYPE_POISON))
                    goto LEFTOVERS;
            case HOLD_EFFECT_STICKY_BARB:
                if (!moveTurn)
                {
                    gBattleMoveDamage = gBattleMons[battlerId].maxHP / 8;
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = 1;
                    BattleScriptExecute(BattleScript_ItemHurtEnd2);
                    effect = ITEM_HP_CHANGE;
                    RecordItemEffectBattle(battlerId, battlerHoldEffect);
                    PREPARE_ITEM_BUFFER(gBattleTextBuff1, gLastUsedItem);
                }
                break;
            case HOLD_EFFECT_LEFTOVERS:
            LEFTOVERS:
                if (gBattleMons[battlerId].hp < gBattleMons[battlerId].maxHP && !moveTurn)
                {
                    gBattleMoveDamage = gBattleMons[battlerId].maxHP / 16;
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = 1;
                    gBattleMoveDamage *= -1;
                    BattleScriptExecute(BattleScript_ItemHealHP_End2);
                    effect = ITEM_HP_CHANGE;
                    RecordItemEffectBattle(battlerId, battlerHoldEffect);
                }
                break;
            case HOLD_EFFECT_CONFUSE_SPICY:
                if (!moveTurn)
                    effect = HealConfuseBerry(battlerId, gLastUsedItem, FLAVOR_SPICY);
                break;
            case HOLD_EFFECT_CONFUSE_DRY:
                if (!moveTurn)
                    effect = HealConfuseBerry(battlerId, gLastUsedItem, FLAVOR_DRY);
                break;
            case HOLD_EFFECT_CONFUSE_SWEET:
                if (!moveTurn)
                    effect = HealConfuseBerry(battlerId, gLastUsedItem, FLAVOR_SWEET);
                break;
            case HOLD_EFFECT_CONFUSE_BITTER:
                if (!moveTurn)
                    effect = HealConfuseBerry(battlerId, gLastUsedItem, FLAVOR_BITTER);
                break;
            case HOLD_EFFECT_CONFUSE_SOUR:
                if (!moveTurn)
                    effect = HealConfuseBerry(battlerId, gLastUsedItem, FLAVOR_SOUR);
                break;
            case HOLD_EFFECT_ATTACK_UP:
                if (!moveTurn)
                    effect = StatRaiseBerry(battlerId, gLastUsedItem, STAT_ATK);
                break;
            case HOLD_EFFECT_DEFENSE_UP:
                if (!moveTurn)
                    effect = StatRaiseBerry(battlerId, gLastUsedItem, STAT_DEF);
                break;
            case HOLD_EFFECT_SPEED_UP:
                if (!moveTurn)
                    effect = StatRaiseBerry(battlerId, gLastUsedItem, STAT_SPEED);
                break;
            case HOLD_EFFECT_SP_ATTACK_UP:
                if (!moveTurn)
                    effect = StatRaiseBerry(battlerId, gLastUsedItem, STAT_SPATK);
                break;
            case HOLD_EFFECT_SP_DEFENSE_UP:
                if (!moveTurn)
                    effect = StatRaiseBerry(battlerId, gLastUsedItem, STAT_SPDEF);
                break;
            case HOLD_EFFECT_CRITICAL_UP:
                if (!moveTurn && !(gBattleMons[battlerId].status2 & STATUS2_FOCUS_ENERGY) && HasEnoughHpToEatBerry(battlerId, GetBattlerHoldEffectParam(battlerId), gLastUsedItem))
                {
                    gBattleMons[battlerId].status2 |= STATUS2_FOCUS_ENERGY;
                    BattleScriptExecute(BattleScript_BerryFocusEnergyEnd2);
                    effect = ITEM_EFFECT_OTHER;
                }
                break;
            case HOLD_EFFECT_RANDOM_STAT_UP:
                if (!moveTurn)
                    effect = RandomStatRaiseBerry(battlerId, gLastUsedItem);
                break;
            case HOLD_EFFECT_CURE_PAR:
                if (gBattleMons[battlerId].status1 & STATUS1_PARALYSIS && !UnnerveOn(battlerId, gLastUsedItem))
                {
                    gBattleMons[battlerId].status1 &= ~(STATUS1_PARALYSIS);
                    BattleScriptExecute(BattleScript_BerryCurePrlzEnd2);
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_PSN:
                if (gBattleMons[battlerId].status1 & STATUS1_PSN_ANY && !UnnerveOn(battlerId, gLastUsedItem))
                {
                    gBattleMons[battlerId].status1 &= ~(STATUS1_PSN_ANY | STATUS1_TOXIC_COUNTER);
                    BattleScriptExecute(BattleScript_BerryCurePsnEnd2);
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_BRN:
                if (gBattleMons[battlerId].status1 & STATUS1_BURN && !UnnerveOn(battlerId, gLastUsedItem))
                {
                    gBattleMons[battlerId].status1 &= ~(STATUS1_BURN);
                    BattleScriptExecute(BattleScript_BerryCureBrnEnd2);
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_FRZ:
                if (gBattleMons[battlerId].status1 & STATUS1_FREEZE && !UnnerveOn(battlerId, gLastUsedItem))
                {
                    gBattleMons[battlerId].status1 &= ~(STATUS1_FREEZE);
                    BattleScriptExecute(BattleScript_BerryCureFrzEnd2);
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_SLP:
                if (gBattleMons[battlerId].status1 & STATUS1_SLEEP && !UnnerveOn(battlerId, gLastUsedItem))
                {
                    gBattleMons[battlerId].status1 &= ~(STATUS1_SLEEP);
                    gBattleMons[battlerId].status2 &= ~(STATUS2_NIGHTMARE);
                    BattleScriptExecute(BattleScript_BerryCureSlpEnd2);
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_CONFUSION:
                if (gBattleMons[battlerId].status2 & STATUS2_CONFUSION && !UnnerveOn(battlerId, gLastUsedItem))
                {
                    gBattleMons[battlerId].status2 &= ~(STATUS2_CONFUSION);
                    BattleScriptExecute(BattleScript_BerryCureConfusionEnd2);
                    effect = ITEM_EFFECT_OTHER;
                }
                break;
            case HOLD_EFFECT_CURE_STATUS:
                if ((gBattleMons[battlerId].status1 & STATUS1_ANY || gBattleMons[battlerId].status2 & STATUS2_CONFUSION) && !UnnerveOn(battlerId, gLastUsedItem))
                {
                    i = 0;
                    if (gBattleMons[battlerId].status1 & STATUS1_PSN_ANY)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_PoisonJpn);
                        i++;
                    }
                    if (gBattleMons[battlerId].status1 & STATUS1_SLEEP)
                    {
                        gBattleMons[battlerId].status2 &= ~(STATUS2_NIGHTMARE);
                        StringCopy(gBattleTextBuff1, gStatusConditionString_SleepJpn);
                        i++;
                    }
                    if (gBattleMons[battlerId].status1 & STATUS1_PARALYSIS)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_ParalysisJpn);
                        i++;
                    }
                    if (gBattleMons[battlerId].status1 & STATUS1_BURN)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_BurnJpn);
                        i++;
                    }
                    if (gBattleMons[battlerId].status1 & STATUS1_FREEZE)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_IceJpn);
                        i++;
                    }
                    if (gBattleMons[battlerId].status2 & STATUS2_CONFUSION)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_ConfusionJpn);
                        i++;
                    }
                    if (!(i > 1))
                        gBattleCommunication[MULTISTRING_CHOOSER] = 0;
                    else
                        gBattleCommunication[MULTISTRING_CHOOSER] = 1;
                    gBattleMons[battlerId].status1 = 0;
                    gBattleMons[battlerId].status2 &= ~(STATUS2_CONFUSION);
                    BattleScriptExecute(BattleScript_BerryCureChosenStatusEnd2);
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_ATTRACT:
                if (gBattleMons[battlerId].status2 & STATUS2_INFATUATION)
                {
                    gBattleMons[battlerId].status2 &= ~(STATUS2_INFATUATION);
                    StringCopy(gBattleTextBuff1, gStatusConditionString_LoveJpn);
                    BattleScriptExecute(BattleScript_BerryCureChosenStatusEnd2);
                    gBattleCommunication[MULTISTRING_CHOOSER] = 0;
                    effect = ITEM_EFFECT_OTHER;
                }
                break;
            }

            if (effect)
            {
                gActiveBattler = gBattlerAttacker = gPotentialItemEffectBattler = gBattleScripting.battler = battlerId;
                switch (effect)
                {
                case ITEM_STATUS_CHANGE:
                    BtlController_EmitSetMonData(0, REQUEST_STATUS_BATTLE, 0, 4, &gBattleMons[battlerId].status1);
                    MarkBattlerForControllerExec(gActiveBattler);
                    break;
                case ITEM_PP_CHANGE:
                    if (!(gBattleMons[battlerId].status2 & STATUS2_TRANSFORMED) && !(gDisableStructs[battlerId].mimickedMoves & gBitTable[i]))
                        gBattleMons[battlerId].pp[i] = changedPP;
                    break;
                }
            }
        }
        break;
    case ITEMEFFECT_MOVE_END:
        for (battlerId = 0; battlerId < gBattlersCount; battlerId++)
        {
            gLastUsedItem = gBattleMons[battlerId].item;
            battlerHoldEffect = GetBattlerHoldEffect(battlerId, TRUE);
            switch (battlerHoldEffect)
            {
            case HOLD_EFFECT_RESTORE_HP:
                if (B_HP_BERRIES >= GEN_4)
                    effect = ItemHealHp(battlerId, gLastUsedItem, FALSE, FALSE);
                break;
            case HOLD_EFFECT_RESTORE_PCT_HP:
                if (B_BERRIES_INSTANT >= GEN_4)
                    effect = ItemHealHp(battlerId, gLastUsedItem, FALSE, TRUE);
                break;
            case HOLD_EFFECT_CURE_PAR:
                if (gBattleMons[battlerId].status1 & STATUS1_PARALYSIS && !UnnerveOn(battlerId, gLastUsedItem))
                {
                    gBattleMons[battlerId].status1 &= ~(STATUS1_PARALYSIS);
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_BerryCureParRet;
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_PSN:
                if (gBattleMons[battlerId].status1 & STATUS1_PSN_ANY && !UnnerveOn(battlerId, gLastUsedItem))
                {
                    gBattleMons[battlerId].status1 &= ~(STATUS1_PSN_ANY | STATUS1_TOXIC_COUNTER);
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_BerryCurePsnRet;
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_BRN:
                if (gBattleMons[battlerId].status1 & STATUS1_BURN && !UnnerveOn(battlerId, gLastUsedItem))
                {
                    gBattleMons[battlerId].status1 &= ~(STATUS1_BURN);
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_BerryCureBrnRet;
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_FRZ:
                if (gBattleMons[battlerId].status1 & STATUS1_FREEZE && !UnnerveOn(battlerId, gLastUsedItem))
                {
                    gBattleMons[battlerId].status1 &= ~(STATUS1_FREEZE);
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_BerryCureFrzRet;
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_SLP:
                if (gBattleMons[battlerId].status1 & STATUS1_SLEEP && !UnnerveOn(battlerId, gLastUsedItem))
                {
                    gBattleMons[battlerId].status1 &= ~(STATUS1_SLEEP);
                    gBattleMons[battlerId].status2 &= ~(STATUS2_NIGHTMARE);
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_BerryCureSlpRet;
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_CURE_CONFUSION:
                if (gBattleMons[battlerId].status2 & STATUS2_CONFUSION && !UnnerveOn(battlerId, gLastUsedItem))
                {
                    gBattleMons[battlerId].status2 &= ~(STATUS2_CONFUSION);
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_BerryCureConfusionRet;
                    effect = ITEM_EFFECT_OTHER;
                }
                break;
            case HOLD_EFFECT_CURE_ATTRACT:
                if (gBattleMons[battlerId].status2 & STATUS2_INFATUATION)
                {
                    gBattleMons[battlerId].status2 &= ~(STATUS2_INFATUATION);
                    StringCopy(gBattleTextBuff1, gStatusConditionString_LoveJpn);
                    BattleScriptPushCursor();
                    gBattleCommunication[MULTISTRING_CHOOSER] = 0;
                    gBattlescriptCurrInstr = BattleScript_BerryCureChosenStatusRet;
                    effect = ITEM_EFFECT_OTHER;
                }
                break;
            case HOLD_EFFECT_CURE_STATUS:
                if ((gBattleMons[battlerId].status1 & STATUS1_ANY || gBattleMons[battlerId].status2 & STATUS2_CONFUSION) && !UnnerveOn(battlerId, gLastUsedItem))
                {
                    if (gBattleMons[battlerId].status1 & STATUS1_PSN_ANY)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_PoisonJpn);
                    }
                    if (gBattleMons[battlerId].status1 & STATUS1_SLEEP)
                    {
                        gBattleMons[battlerId].status2 &= ~(STATUS2_NIGHTMARE);
                        StringCopy(gBattleTextBuff1, gStatusConditionString_SleepJpn);
                    }
                    if (gBattleMons[battlerId].status1 & STATUS1_PARALYSIS)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_ParalysisJpn);
                    }
                    if (gBattleMons[battlerId].status1 & STATUS1_BURN)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_BurnJpn);
                    }
                    if (gBattleMons[battlerId].status1 & STATUS1_FREEZE)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_IceJpn);
                    }
                    if (gBattleMons[battlerId].status2 & STATUS2_CONFUSION)
                    {
                        StringCopy(gBattleTextBuff1, gStatusConditionString_ConfusionJpn);
                    }
                    gBattleMons[battlerId].status1 = 0;
                    gBattleMons[battlerId].status2 &= ~(STATUS2_CONFUSION);
                    BattleScriptPushCursor();
                    gBattleCommunication[MULTISTRING_CHOOSER] = 0;
                    gBattlescriptCurrInstr = BattleScript_BerryCureChosenStatusRet;
                    effect = ITEM_STATUS_CHANGE;
                }
                break;
            case HOLD_EFFECT_RESTORE_STATS:
                for (i = 0; i < NUM_BATTLE_STATS; i++)
                {
                    if (gBattleMons[battlerId].statStages[i] < DEFAULT_STAT_STAGE)
                    {
                        gBattleMons[battlerId].statStages[i] = DEFAULT_STAT_STAGE;
                        effect = ITEM_STATS_CHANGE;
                    }
                }
                if (effect)
                {
                    gBattleScripting.battler = battlerId;
                    gPotentialItemEffectBattler = battlerId;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_WhiteHerbRet;
                    return effect;
                }
                break;
            }

            if (effect)
            {
                gActiveBattler = gPotentialItemEffectBattler = gBattleScripting.battler = battlerId;
                if (effect == ITEM_STATUS_CHANGE)
                {
                    BtlController_EmitSetMonData(0, REQUEST_STATUS_BATTLE, 0, 4, &gBattleMons[gActiveBattler].status1);
                    MarkBattlerForControllerExec(gActiveBattler);
                }
                break;
            }
        }
        break;
    case ITEMEFFECT_KINGSROCK_SHELLBELL:
        if (gBattleMoveDamage)
        {
            switch (atkHoldEffect)
            {
            case HOLD_EFFECT_FLINCH:
                if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                    && TARGET_TURN_DAMAGED
                    && (Random() % 100) < atkHoldEffectParam
                    && gBattleMoves[gCurrentMove].flags & FLAG_KINGSROCK_AFFECTED
                    && gBattleMons[gBattlerTarget].hp)
                {
                    gBattleScripting.moveEffect = MOVE_EFFECT_FLINCH;
                    BattleScriptPushCursor();
                    SetMoveEffect(FALSE, 0);
                    BattleScriptPop();
                }
                break;
            case HOLD_EFFECT_SHELL_BELL:
                if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
                    && gSpecialStatuses[gBattlerTarget].dmg != 0
                    && gSpecialStatuses[gBattlerTarget].dmg != 0xFFFF
                    && gBattlerAttacker != gBattlerTarget
                    && gBattleMons[gBattlerAttacker].hp != gBattleMons[gBattlerAttacker].maxHP
                    && gBattleMons[gBattlerAttacker].hp != 0)
                {
                    gLastUsedItem = atkItem;
                    gPotentialItemEffectBattler = gBattlerAttacker;
                    gBattleScripting.battler = gBattlerAttacker;
                    gBattleMoveDamage = (gSpecialStatuses[gBattlerTarget].dmg / atkHoldEffectParam) * -1;
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = -1;
                    gSpecialStatuses[gBattlerTarget].dmg = 0;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_ItemHealHP_Ret;
                    effect++;
                }
                break;
            }
        }
        break;
    case ITEMEFFECT_TARGET:
        if (!(gMoveResultFlags & MOVE_RESULT_NO_EFFECT))
        {
            GET_MOVE_TYPE(gCurrentMove, moveType);
            switch (battlerHoldEffect)
            {
            case HOLD_EFFECT_AIR_BALLOON:
                if (TARGET_TURN_DAMAGED)
                {
                    effect = ITEM_EFFECT_OTHER;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_AirBaloonMsgPop;
                }
                break;
            case HOLD_EFFECT_ROCKY_HELMET:
                if (TARGET_TURN_DAMAGED
                    && IsMoveMakingContact(gCurrentMove, gBattlerAttacker)
                    && IsBattlerAlive(gBattlerAttacker)
                    && GetBattlerAbility(gBattlerAttacker) != ABILITY_MAGIC_GUARD)
                {
                    gBattleMoveDamage = gBattleMons[gBattlerAttacker].maxHP / 6;
                    if (gBattleMoveDamage == 0)
                        gBattleMoveDamage = 1;
                    effect = ITEM_HP_CHANGE;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_RockyHelmetActivates;
                    PREPARE_ITEM_BUFFER(gBattleTextBuff1, gLastUsedItem);
                    RecordItemEffectBattle(battlerId, HOLD_EFFECT_ROCKY_HELMET);
                }
                break;
            case HOLD_EFFECT_WEAKNESS_POLICY:
                if (IsBattlerAlive(battlerId)
                    && TARGET_TURN_DAMAGED
                    && gMoveResultFlags & MOVE_RESULT_SUPER_EFFECTIVE)
                {
                    effect = ITEM_STATS_CHANGE;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_WeaknessPolicy;
                }
                break;
            case HOLD_EFFECT_SNOWBALL:
                if (IsBattlerAlive(battlerId)
                    && TARGET_TURN_DAMAGED
                    && moveType == TYPE_ICE)
                {
                    effect = ITEM_STATS_CHANGE;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_TargetItemStatRaise;
                    gBattleScripting.statChanger = SET_STATCHANGER(STAT_ATK, 1, FALSE);
                }
                break;
            case HOLD_EFFECT_LUMINOUS_MOSS:
                if (IsBattlerAlive(battlerId)
                    && TARGET_TURN_DAMAGED
                    && moveType == TYPE_WATER)
                {
                    effect = ITEM_STATS_CHANGE;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_TargetItemStatRaise;
                    gBattleScripting.statChanger = SET_STATCHANGER(STAT_SPDEF, 1, FALSE);
                }
                break;
            case HOLD_EFFECT_CELL_BATTERY:
                if (IsBattlerAlive(battlerId)
                    && TARGET_TURN_DAMAGED
                    && moveType == TYPE_ELECTRIC)
                {
                    effect = ITEM_STATS_CHANGE;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_TargetItemStatRaise;
                    gBattleScripting.statChanger = SET_STATCHANGER(STAT_ATK, 1, FALSE);
                }
                break;
            case HOLD_EFFECT_ABSORB_BULB:
                if (IsBattlerAlive(battlerId)
                    && TARGET_TURN_DAMAGED
                    && moveType == TYPE_WATER)
                {
                    effect = ITEM_STATS_CHANGE;
                    BattleScriptPushCursor();
                    gBattlescriptCurrInstr = BattleScript_TargetItemStatRaise;
                    gBattleScripting.statChanger = SET_STATCHANGER(STAT_SPATK, 1, FALSE);
                }
                break;
            }
        }
        break;
    case ITEMEFFECT_ORBS:
        switch (battlerHoldEffect)
        {
        case HOLD_EFFECT_TOXIC_ORB:
            if (CanBePoisoned(battlerId, battlerId))
            {
                effect = ITEM_STATUS_CHANGE;
                gBattleMons[battlerId].status1 = STATUS1_TOXIC_POISON;
                BattleScriptExecute(BattleScript_ToxicOrb);
                RecordItemEffectBattle(battlerId, battlerHoldEffect);
            }
            break;
        case HOLD_EFFECT_FLAME_ORB:
            if (!gBattleMons[battlerId].status1
                && !IS_BATTLER_OF_TYPE(battlerId, TYPE_FIRE)
				&& !(gFieldStatuses & STATUS_FIELD_WATERSPORT)
                && GetBattlerAbility(battlerId) != ABILITY_WATER_VEIL)
            {
                effect = ITEM_STATUS_CHANGE;
                gBattleMons[battlerId].status1 = STATUS1_BURN;
                BattleScriptExecute(BattleScript_FlameOrb);
                RecordItemEffectBattle(battlerId, battlerHoldEffect);
            }
            break;
        }

        if (effect == ITEM_STATUS_CHANGE)
        {
            gActiveBattler = battlerId;
            BtlController_EmitSetMonData(0, REQUEST_STATUS_BATTLE, 0, 4, &gBattleMons[battlerId].status1);
            MarkBattlerForControllerExec(gActiveBattler);
        }
        break;
    }

    // Berry was successfully used on a Pokemon.
    if (effect && (gLastUsedItem >= FIRST_BERRY_INDEX && gLastUsedItem <= LAST_BERRY_INDEX))
        gBattleStruct->ateBerry[battlerId & BIT_SIDE] |= gBitTable[gBattlerPartyIndexes[battlerId]];

    return effect;
}

void ClearFuryCutterDestinyBondGrudge(u8 battlerId)
{
    gDisableStructs[battlerId].furyCutterCounter = 0;
    gBattleMons[battlerId].status2 &= ~(STATUS2_DESTINY_BOND);
    gStatuses3[battlerId] &= ~(STATUS3_GRUDGE);
}

void HandleAction_RunBattleScript(void) // identical to RunBattleScriptCommands
{
    if (gBattleControllerExecFlags == 0)
        gBattleScriptingCommandsTable[*gBattlescriptCurrInstr]();
}

u32 SetRandomTarget(u32 battlerId)
{
    u32 target;
    static const u8 targets[2][2] =
    {
        [B_SIDE_PLAYER] = {B_POSITION_OPPONENT_LEFT, B_POSITION_OPPONENT_RIGHT},
        [B_SIDE_OPPONENT] = {B_POSITION_PLAYER_LEFT, B_POSITION_PLAYER_RIGHT},
    };

    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
    {
        target = GetBattlerAtPosition(targets[GetBattlerSide(battlerId)][Random() % 2]);
        if (!IsBattlerAlive(target))
            target ^= BIT_FLANK;
    }
    else
    {
        target = GetBattlerAtPosition(targets[GetBattlerSide(battlerId)][0]);
    }

    return target;
}

u8 GetMoveTarget(u16 move, u8 setTarget)
{
    u8 targetBattler = 0;
    u32 i, moveTarget, side;

    if (setTarget)
        moveTarget = setTarget - 1;
    else
        moveTarget = gBattleMoves[move].target;

    switch (moveTarget)
    {
    case MOVE_TARGET_SELECTED:
        side = GetBattlerSide(gBattlerAttacker) ^ BIT_SIDE;
        if (gSideTimers[side].followmeTimer && gBattleMons[gSideTimers[side].followmeTarget].hp)
        {
            targetBattler = gSideTimers[side].followmeTarget;
        }
        else
        {
            targetBattler = SetRandomTarget(gBattlerAttacker);
            if (gBattleMoves[move].type == TYPE_ELECTRIC
                && IsAbilityOnOpposingSide(gBattlerAttacker, ABILITY_LIGHTNING_ROD)
                && gBattleMons[targetBattler].ability != ABILITY_LIGHTNING_ROD)
            {
                targetBattler ^= BIT_FLANK;
                RecordAbilityBattle(targetBattler, gBattleMons[targetBattler].ability);
                gSpecialStatuses[targetBattler].lightningRodRedirected = 1;
            }
            else if (gBattleMoves[move].type == TYPE_WATER
                && IsAbilityOnOpposingSide(gBattlerAttacker, ABILITY_STORM_DRAIN)
                && gBattleMons[targetBattler].ability != ABILITY_STORM_DRAIN)
            {
                targetBattler ^= BIT_FLANK;
                RecordAbilityBattle(targetBattler, gBattleMons[targetBattler].ability);
                gSpecialStatuses[targetBattler].stormDrainRedirected = 1;
            }
        }
        break;
    case MOVE_TARGET_DEPENDS:
    case MOVE_TARGET_BOTH:
    case MOVE_TARGET_FOES_AND_ALLY:
    case MOVE_TARGET_OPPONENTS_FIELD:
        targetBattler = GetBattlerAtPosition((GetBattlerPosition(gBattlerAttacker) & BIT_SIDE) ^ BIT_SIDE);
        if (!IsBattlerAlive(targetBattler))
            targetBattler ^= BIT_FLANK;
        break;
    case MOVE_TARGET_RANDOM:
        side = GetBattlerSide(gBattlerAttacker) ^ BIT_SIDE;
        if (gSideTimers[side].followmeTimer && gBattleMons[gSideTimers[side].followmeTarget].hp)
            targetBattler = gSideTimers[side].followmeTarget;
        else if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE && moveTarget & MOVE_TARGET_RANDOM)
            targetBattler = SetRandomTarget(gBattlerAttacker);
        else
            targetBattler = GetBattlerAtPosition((GetBattlerPosition(gBattlerAttacker) & BIT_SIDE) ^ BIT_SIDE);
        break;
    case MOVE_TARGET_USER_OR_SELECTED:
    case MOVE_TARGET_USER:
    default:
        targetBattler = gBattlerAttacker;
        break;
    case MOVE_TARGET_ALLY:
        if (IsBattlerAlive(BATTLE_PARTNER(gBattlerAttacker)))
            targetBattler = BATTLE_PARTNER(gBattlerAttacker);
        else
            targetBattler = gBattlerAttacker;
        break;
    }

    *(gBattleStruct->moveTarget + gBattlerAttacker) = targetBattler;

    return targetBattler;
}

static bool32 HasObedientBitSet(u8 battlerId)
{
    if (GetBattlerSide(battlerId) == B_SIDE_OPPONENT)
        return TRUE;
    if (GetMonData(&gPlayerParty[gBattlerPartyIndexes[battlerId]], MON_DATA_SPECIES, NULL) != SPECIES_DEOXYS
        && GetMonData(&gPlayerParty[gBattlerPartyIndexes[battlerId]], MON_DATA_SPECIES, NULL) != SPECIES_MEW)
            return TRUE;
    return GetMonData(&gPlayerParty[gBattlerPartyIndexes[battlerId]], MON_DATA_OBEDIENCE, NULL);
}

u8 IsMonDisobedient(void)
{
    /*/s32 rnd;
    s32 calc;
    u8 obedienceLevel = 0;

    if (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_x2000000))
        return 0;
    if (GetBattlerSide(gBattlerAttacker) == B_SIDE_OPPONENT)
        return 0;

    if (HasObedientBitSet(gBattlerAttacker)) // only if species is Mew or Deoxys
    {
        if (gBattleTypeFlags & BATTLE_TYPE_INGAME_PARTNER && GetBattlerPosition(gBattlerAttacker) == 2)
            return 0;
        if (gBattleTypeFlags & BATTLE_TYPE_FRONTIER)
            return 0;
        if (gBattleTypeFlags & BATTLE_TYPE_RECORDED)
            return 0;
        if (!IsOtherTrainer(gBattleMons[gBattlerAttacker].otId, gBattleMons[gBattlerAttacker].otName))
            return 0;
        if (FlagGet(FLAG_BADGE08_GET))
            return 0;

        obedienceLevel = 10;

        if (FlagGet(FLAG_BADGE02_GET))
            obedienceLevel = 30;
        if (FlagGet(FLAG_BADGE04_GET))
            obedienceLevel = 50;
        if (FlagGet(FLAG_BADGE06_GET))
            obedienceLevel = 70;
    }

    if (gBattleMons[gBattlerAttacker].level <= obedienceLevel)
        return 0;
    rnd = (Random() & 255);
    calc = (gBattleMons[gBattlerAttacker].level + obedienceLevel) * rnd >> 8;
    if (calc < obedienceLevel)
        return 0;

    // is not obedient
    if (gCurrentMove == MOVE_RAGE)
        gBattleMons[gBattlerAttacker].status2 &= ~(STATUS2_RAGE);
    if (gBattleMons[gBattlerAttacker].status1 & STATUS1_SLEEP && (gCurrentMove == MOVE_SNORE || gCurrentMove == MOVE_SLEEP_TALK))
    {
        gBattlescriptCurrInstr = BattleScript_IgnoresWhileAsleep;
        return 1;
    }

    rnd = (Random() & 255);
    calc = (gBattleMons[gBattlerAttacker].level + obedienceLevel) * rnd >> 8;
    if (calc < obedienceLevel)
    {
        calc = CheckMoveLimitations(gBattlerAttacker, gBitTable[gCurrMovePos], 0xFF);
        if (calc == 0xF) // all moves cannot be used
        {
            gBattleCommunication[MULTISTRING_CHOOSER] = Random() & 3;
            gBattlescriptCurrInstr = BattleScript_MoveUsedLoafingAround;
            return 1;
        }
        else // use a random move
        {
            do
            {
                gCurrMovePos = gChosenMovePos = Random() & 3;
            } while (gBitTable[gCurrMovePos] & calc);

            gCalledMove = gBattleMons[gBattlerAttacker].moves[gCurrMovePos];
            gBattlescriptCurrInstr = BattleScript_IgnoresAndUsesRandomMove;
            gBattlerTarget = GetMoveTarget(gCalledMove, 0);
            gHitMarker |= HITMARKER_x200000;
            return 2;
        }
    }
    else
    {
        obedienceLevel = gBattleMons[gBattlerAttacker].level - obedienceLevel;

        calc = (Random() & 255);
        if (calc < obedienceLevel && !(gBattleMons[gBattlerAttacker].status1 & STATUS1_ANY) && gBattleMons[gBattlerAttacker].ability != ABILITY_VITAL_SPIRIT && gBattleMons[gBattlerAttacker].ability != ABILITY_INSOMNIA)
        {
            // try putting asleep
            int i;
            for (i = 0; i < gBattlersCount; i++)
            {
                if (gBattleMons[i].status2 & STATUS2_UPROAR)
                    break;
            }
            if (i == gBattlersCount)
            {
                gBattlescriptCurrInstr = BattleScript_IgnoresAndFallsAsleep;
                return 1;
            }
        }
        calc -= obedienceLevel;
        if (calc < obedienceLevel)
        {
            gBattleMoveDamage = CalculateMoveDamage(MOVE_NONE, gBattlerAttacker, gBattlerAttacker, TYPE_MYSTERY, 40, FALSE, FALSE, TRUE);
            gBattlerTarget = gBattlerAttacker;
            gBattlescriptCurrInstr = BattleScript_IgnoresAndHitsItself;
            gHitMarker |= HITMARKER_UNABLE_TO_USE_MOVE;
            return 2;
        }
        else
        {
            gBattleCommunication[MULTISTRING_CHOOSER] = Random() & 3;
            gBattlescriptCurrInstr = BattleScript_MoveUsedLoafingAround;
            return 1;
        }
    }/*/
	return 0;
}

u32 GetBattlerHoldEffect(u8 battlerId, bool32 checkNegating)
{
    if (checkNegating)
    {
        if (gStatuses3[battlerId] & STATUS3_EMBARGO)
            return HOLD_EFFECT_NONE;
        if (gFieldStatuses & STATUS_FIELD_MAGIC_ROOM)
            return HOLD_EFFECT_NONE;
        if (gBattleMons[battlerId].ability == ABILITY_KLUTZ && !(gStatuses3[battlerId] & STATUS3_GASTRO_ACID))
            return HOLD_EFFECT_NONE;
    }

    gPotentialItemEffectBattler = battlerId;

    if (USE_BATTLE_DEBUG && gBattleStruct->debugHoldEffects[battlerId] != 0 && gBattleMons[battlerId].item)
        return gBattleStruct->debugHoldEffects[battlerId];
    else if (gBattleMons[battlerId].item == ITEM_ENIGMA_BERRY)
        return gEnigmaBerries[battlerId].holdEffect;
    else
        return ItemId_GetHoldEffect(gBattleMons[battlerId].item);
}

u32 GetBattlerHoldEffectParam(u8 battlerId)
{
    if (gBattleMons[battlerId].item == ITEM_ENIGMA_BERRY)
        return gEnigmaBerries[battlerId].holdEffectParam;
    else
        return ItemId_GetHoldEffectParam(gBattleMons[battlerId].item);
}

bool32 IsMoveMakingContact(u16 move, u8 battlerAtk)
{
    if (!(gBattleMoves[move].flags & FLAG_MAKES_CONTACT))
        return FALSE;
    else if (GetBattlerAbility(battlerAtk) == ABILITY_LONG_REACH)
        return FALSE;
    else if (GetBattlerHoldEffect(battlerAtk, TRUE) == HOLD_EFFECT_PROTECTIVE_PADS)
        return FALSE;
    else
        return TRUE;
}

bool32 IsBattlerGrounded(u8 battlerId)
{
    u16 speciesId = GetFormSpeciesId(gBattleMons[battlerId].species, gBattleMons[battlerId].formId);
	
	if (GetBattlerHoldEffect(battlerId, TRUE) == HOLD_EFFECT_IRON_BALL)
        return TRUE;
    else if (gFieldStatuses & STATUS_FIELD_GRAVITY)
        return TRUE;
    else if (gStatuses3[battlerId] & STATUS3_ROOTED)
        return TRUE;
    else if (gStatuses3[battlerId] & STATUS3_SMACKED_DOWN)
        return TRUE;

    else if (gStatuses3[battlerId] & STATUS3_TELEKINESIS)
        return FALSE;
    else if (gStatuses3[battlerId] & STATUS3_MAGNET_RISE)
        return FALSE;
    else if (GetBattlerHoldEffect(battlerId, TRUE) == HOLD_EFFECT_AIR_BALLOON)
        return FALSE;
    else if (GetBattlerAbility(battlerId) == ABILITY_LEVITATE)
        return FALSE;
	else if (gBaseStats[gBattleMons[battlerId].species].flags & F_GROUND_INMUNITY)
		return FALSE;
    else if (IS_BATTLER_OF_TYPE(battlerId, TYPE_FLYING) && !FlagGet(B_FLAG_INVERSE_BATTLE))
        return FALSE;

    else
        return TRUE;
}

bool32 IsBattlerAlive(u8 battlerId)
{
    if (gBattleMons[battlerId].hp == 0)
        return FALSE;
    else if (battlerId >= gBattlersCount)
        return FALSE;
    else if (gAbsentBattlerFlags & gBitTable[battlerId])
        return FALSE;
    else
        return TRUE;
}

u8 GetBattleMonMoveSlot(struct BattlePokemon *battleMon, u16 move)
{
    u8 i;

    for (i = 0; i < 4; i++)
    {
        if (battleMon->moves[i] == move)
            break;
    }
    return i;
}

u32 GetBattlerWeight(u8 battlerId)
{
    u32 i;
    u32 weight = GetPokedexHeightWeight(SpeciesToNationalPokedexNum(gBattleMons[battlerId].species), 1);
    u32 ability = GetBattlerAbility(battlerId);
    u32 holdEffect = GetBattlerHoldEffect(battlerId, TRUE);

    if (ability == ABILITY_HEAVY_METAL)
        weight *= 2;
    else if (ability == ABILITY_LIGHT_METAL)
        weight /= 2;

    if (holdEffect == HOLD_EFFECT_FLOAT_STONE)
        weight /= 2;

    for (i = 0; i < gDisableStructs[battlerId].autotomizeCount; i++)
    {
        if (weight > 1000)
        {
            weight -= 1000;
        }
        else if (weight <= 1000)
        {
            weight = 1;
            break;
        }
    }

    if (weight == 0)
        weight = 1;

    return weight;
}

u32 CountBattlerStatIncreases(u8 battlerId, bool32 countEvasionAcc)
{
    u32 i;
    u32 count = 0;

    for (i = 0; i < NUM_BATTLE_STATS; i++)
    {
        if ((i == STAT_ACC || i == STAT_EVASION) && !countEvasionAcc)
            continue;
        if (gBattleMons[battlerId].statStages[i] > 6) // Stat is increased.
            count += gBattleMons[battlerId].statStages[i] - 6;
    }

    return count;
}

u32 GetMoveTargetCount(u16 move, u8 battlerAtk, u8 battlerDef)
{
    switch (gBattleMoves[move].target)
    {
    case MOVE_TARGET_BOTH:
        return IsBattlerAlive(battlerDef)
             + IsBattlerAlive(BATTLE_PARTNER(battlerDef));
    case MOVE_TARGET_FOES_AND_ALLY:
        return IsBattlerAlive(battlerDef)
             + IsBattlerAlive(BATTLE_PARTNER(battlerDef))
             + IsBattlerAlive(BATTLE_PARTNER(battlerAtk));
    case MOVE_TARGET_OPPONENTS_FIELD:
        return 1;
    case MOVE_TARGET_DEPENDS:
    case MOVE_TARGET_SELECTED:
    case MOVE_TARGET_RANDOM:
    case MOVE_TARGET_USER_OR_SELECTED:
        return IsBattlerAlive(battlerDef);
    case MOVE_TARGET_USER:
        return IsBattlerAlive(battlerAtk);
    default:
        return 0;
    }
}

static void MulModifier(u16 *modifier, u16 val)
{
    *modifier = UQ_4_12_TO_INT((*modifier * val) + UQ_4_12_ROUND);
}

static u32 ApplyModifier(u16 modifier, u32 val)
{
    return UQ_4_12_TO_INT((modifier * val) + UQ_4_12_ROUND);
}

static const u8 sFlailHpScaleToPowerTable[] =
{
    1, 200,
    4, 150,
    9, 100,
    16, 80,
    32, 40,
    48, 20
};

// format: min. weight (hectograms), base power
static const u16 sWeightToDamageTable[] =
{
    100, 20,
    250, 40,
    500, 60,
    1000, 80,
    2000, 100,
    0xFFFF, 0xFFFF
};

static const u8 sSpeedDiffPowerTable[] = {40, 60, 80, 120, 150};
static const u8 sHeatCrushPowerTable[] = {40, 40, 60, 80, 100, 120};
static const u8 sTrumpCardPowerTable[] = {200, 80, 60, 50, 40};

const struct TypePower gNaturalGiftTable[] =
{
    [ITEM_TO_BERRY(ITEM_CHERI_BERRY)] = {TYPE_FIRE, 80},
    [ITEM_TO_BERRY(ITEM_CHESTO_BERRY)] = {TYPE_WATER, 80},
    [ITEM_TO_BERRY(ITEM_PECHA_BERRY)] = {TYPE_ELECTRIC, 80},
    [ITEM_TO_BERRY(ITEM_RAWST_BERRY)] = {TYPE_GRASS, 80},
    [ITEM_TO_BERRY(ITEM_ASPEAR_BERRY)] = {TYPE_ICE, 80},
    [ITEM_TO_BERRY(ITEM_LEPPA_BERRY)] = {TYPE_FIGHTING, 80},
    [ITEM_TO_BERRY(ITEM_ORAN_BERRY)] = {TYPE_POISON, 80},
    [ITEM_TO_BERRY(ITEM_PERSIM_BERRY)] = {TYPE_GROUND, 80},
    [ITEM_TO_BERRY(ITEM_LUM_BERRY)] = {TYPE_FLYING, 80},
    [ITEM_TO_BERRY(ITEM_SITRUS_BERRY)] = {TYPE_PSYCHIC, 80},
    [ITEM_TO_BERRY(ITEM_FIGY_BERRY)] = {TYPE_BUG, 80},
    [ITEM_TO_BERRY(ITEM_WIKI_BERRY)] = {TYPE_ROCK, 80},
    [ITEM_TO_BERRY(ITEM_MAGO_BERRY)] = {TYPE_GHOST, 80},
    [ITEM_TO_BERRY(ITEM_AGUAV_BERRY)] = {TYPE_DRAGON, 80},
    [ITEM_TO_BERRY(ITEM_IAPAPA_BERRY)] = {TYPE_DARK, 80},
    [ITEM_TO_BERRY(ITEM_RAZZ_BERRY)] = {TYPE_STEEL, 80},
    [ITEM_TO_BERRY(ITEM_OCCA_BERRY)] = {TYPE_FIRE, 80},
    [ITEM_TO_BERRY(ITEM_PASSHO_BERRY)] = {TYPE_WATER, 80},
    [ITEM_TO_BERRY(ITEM_WACAN_BERRY)] = {TYPE_ELECTRIC, 80},
    [ITEM_TO_BERRY(ITEM_RINDO_BERRY)] = {TYPE_GRASS, 80},
    [ITEM_TO_BERRY(ITEM_YACHE_BERRY)] = {TYPE_ICE, 80},
    [ITEM_TO_BERRY(ITEM_CHOPLE_BERRY)] = {TYPE_FIGHTING, 80},
    [ITEM_TO_BERRY(ITEM_KEBIA_BERRY)] = {TYPE_POISON, 80},
    [ITEM_TO_BERRY(ITEM_SHUCA_BERRY)] = {TYPE_GROUND, 80},
    [ITEM_TO_BERRY(ITEM_COBA_BERRY)] = {TYPE_FLYING, 80},
    [ITEM_TO_BERRY(ITEM_PAYAPA_BERRY)] = {TYPE_PSYCHIC, 80},
    [ITEM_TO_BERRY(ITEM_TANGA_BERRY)] = {TYPE_BUG, 80},
    [ITEM_TO_BERRY(ITEM_CHARTI_BERRY)] = {TYPE_ROCK, 80},
    [ITEM_TO_BERRY(ITEM_KASIB_BERRY)] = {TYPE_GHOST, 80},
    [ITEM_TO_BERRY(ITEM_HABAN_BERRY)] = {TYPE_DRAGON, 80},
    [ITEM_TO_BERRY(ITEM_COLBUR_BERRY)] = {TYPE_DARK, 80},
    [ITEM_TO_BERRY(ITEM_BABIRI_BERRY)] = {TYPE_STEEL, 80},
    [ITEM_TO_BERRY(ITEM_CHILAN_BERRY)] = {TYPE_NORMAL, 80},
    [ITEM_TO_BERRY(ITEM_ROSELI_BERRY)] = {TYPE_FAIRY, 80},
    [ITEM_TO_BERRY(ITEM_BLUK_BERRY)] = {TYPE_FIRE, 90},
    [ITEM_TO_BERRY(ITEM_NANAB_BERRY)] = {TYPE_WATER, 90},
    [ITEM_TO_BERRY(ITEM_WEPEAR_BERRY)] = {TYPE_ELECTRIC, 90},
    [ITEM_TO_BERRY(ITEM_PINAP_BERRY)] = {TYPE_GRASS, 90},
    [ITEM_TO_BERRY(ITEM_POMEG_BERRY)] = {TYPE_ICE, 90},
    [ITEM_TO_BERRY(ITEM_KELPSY_BERRY)] = {TYPE_FIGHTING, 90},
    [ITEM_TO_BERRY(ITEM_QUALOT_BERRY)] = {TYPE_POISON, 90},
    [ITEM_TO_BERRY(ITEM_HONDEW_BERRY)] = {TYPE_GROUND, 90},
    [ITEM_TO_BERRY(ITEM_GREPA_BERRY)] = {TYPE_FLYING, 90},
    [ITEM_TO_BERRY(ITEM_TAMATO_BERRY)] = {TYPE_PSYCHIC, 90},
    [ITEM_TO_BERRY(ITEM_CORNN_BERRY)] = {TYPE_BUG, 90},
    [ITEM_TO_BERRY(ITEM_MAGOST_BERRY)] = {TYPE_ROCK, 90},
    [ITEM_TO_BERRY(ITEM_RABUTA_BERRY)] = {TYPE_GHOST, 90},
    [ITEM_TO_BERRY(ITEM_NOMEL_BERRY)] = {TYPE_DRAGON, 90},
    [ITEM_TO_BERRY(ITEM_SPELON_BERRY)] = {TYPE_DARK, 90},
    [ITEM_TO_BERRY(ITEM_PAMTRE_BERRY)] = {TYPE_STEEL, 90},
    [ITEM_TO_BERRY(ITEM_WATMEL_BERRY)] = {TYPE_FIRE, 100},
    [ITEM_TO_BERRY(ITEM_DURIN_BERRY)] = {TYPE_WATER, 100},
    [ITEM_TO_BERRY(ITEM_BELUE_BERRY)] = {TYPE_ELECTRIC, 100},
    [ITEM_TO_BERRY(ITEM_LIECHI_BERRY)] = {TYPE_GRASS, 100},
    [ITEM_TO_BERRY(ITEM_GANLON_BERRY)] = {TYPE_ICE, 100},
    [ITEM_TO_BERRY(ITEM_SALAC_BERRY)] = {TYPE_FIGHTING, 100},
    [ITEM_TO_BERRY(ITEM_PETAYA_BERRY)] = {TYPE_POISON, 100},
    [ITEM_TO_BERRY(ITEM_APICOT_BERRY)] = {TYPE_GROUND, 100},
    [ITEM_TO_BERRY(ITEM_LANSAT_BERRY)] = {TYPE_FLYING, 100},
    [ITEM_TO_BERRY(ITEM_STARF_BERRY)] = {TYPE_PSYCHIC, 100},
    [ITEM_TO_BERRY(ITEM_ENIGMA_BERRY)] = {TYPE_BUG, 100},
    [ITEM_TO_BERRY(ITEM_MICLE_BERRY)] = {TYPE_ROCK, 100},
    [ITEM_TO_BERRY(ITEM_CUSTAP_BERRY)] = {TYPE_GHOST, 100},
    [ITEM_TO_BERRY(ITEM_JABOCA_BERRY)] = {TYPE_DRAGON, 100},
    [ITEM_TO_BERRY(ITEM_ROWAP_BERRY)] = {TYPE_DARK, 100},
    [ITEM_TO_BERRY(ITEM_KEE_BERRY)] = {TYPE_FAIRY, 100},
    [ITEM_TO_BERRY(ITEM_MARANGA_BERRY)] = {TYPE_DARK, 100},
};

static u16 CalcMoveBasePower(u16 move, u8 battlerAtk, u8 battlerDef)
{
    u32 i;
    u16 basePower = gBattleMoves[move].power;
    u32 weight, hpFraction, speed;
	u16 speciesId = GetFormSpeciesId(gBattleMons[battlerAtk].species, gBattleMons[battlerAtk].formId);
	u8 powerLimit = GetCurrentMovePowerLimit();

    switch (gBattleMoves[move].effect)
    {
    case EFFECT_PLEDGE:
        // todo
        break;
    case EFFECT_FLING:
        // todo: program Fling + Unburden interaction
        break;
    case EFFECT_ERUPTION:
        basePower = gBattleMons[battlerAtk].hp * basePower / gBattleMons[battlerAtk].maxHP;
        break;
    case EFFECT_FLAIL:
        hpFraction = GetScaledHPFraction(gBattleMons[battlerAtk].hp, gBattleMons[battlerAtk].maxHP, 48);
        for (i = 0; i < sizeof(sFlailHpScaleToPowerTable); i += 2)
        {
            if (hpFraction <= sFlailHpScaleToPowerTable[i])
                break;
        }
        basePower = sFlailHpScaleToPowerTable[i + 1];
        break;
    case EFFECT_RETURN:
        basePower = 10 * (gBattleMons[battlerAtk].friendship) / 25;
        break;
    case EFFECT_FRUSTRATION:
        basePower = 10 * (255 - gBattleMons[battlerAtk].friendship) / 25;
        break;
    case EFFECT_FURY_CUTTER:
        for (i = 1; i < gDisableStructs[battlerAtk].furyCutterCounter; i++)
            basePower *= 2;
        break;
    case EFFECT_ROLLOUT:
        for (i = 1; i < (5 - gDisableStructs[battlerAtk].rolloutTimer); i++)
            basePower *= 2;
        if (gBattleMons[battlerAtk].status2 & STATUS2_DEFENSE_CURL)
            basePower *= 2;
        break;
    case EFFECT_MAGNITUDE:
        basePower = gBattleStruct->magnitudeBasePower;
        break;
    case EFFECT_PRESENT:
        basePower = gBattleStruct->presentBasePower;
        break;
    case EFFECT_TRIPLE_KICK:
        basePower += gBattleScripting.tripleKickPower;
        break;
    case EFFECT_SPIT_UP:
        basePower = 100 * gDisableStructs[battlerAtk].stockpileCounter;
        break;
    case EFFECT_REVENGE:
        if ((gProtectStructs[battlerAtk].physicalDmg
                && gProtectStructs[battlerAtk].physicalBattlerId == battlerDef)
            || (gProtectStructs[battlerAtk].specialDmg
                && gProtectStructs[battlerAtk].specialBattlerId == battlerDef))
            basePower *= 2;
        break;
    case EFFECT_WEATHER_BALL:
        if (WEATHER_HAS_EFFECT && gBattleWeather & WEATHER_ANY)
            basePower *= 2;
        break;
    case EFFECT_PURSUIT:
        if (gActionsByTurnOrder[GetBattlerTurnOrderNum(gBattlerTarget)] == B_ACTION_SWITCH)
            basePower *= 2;
        break;
    case EFFECT_NATURAL_GIFT:
        basePower = gNaturalGiftTable[ITEM_TO_BERRY(gBattleMons[battlerAtk].item)].power;
        break;
    case EFFECT_WAKE_UP_SLAP:
        if (gBattleMons[battlerDef].status1 & STATUS1_SLEEP || GetBattlerAbility(battlerDef) == ABILITY_COMATOSE)
            basePower *= 2;
        break;
    case EFFECT_SMELLINGSALT:
        if (gBattleMons[battlerDef].status1 & STATUS1_PARALYSIS)
            basePower *= 2;
        break;
    case EFFECT_WRING_OUT:
        basePower = 120 * gBattleMons[battlerDef].hp / gBattleMons[battlerDef].maxHP;
        break;
    case EFFECT_HEX:
        if (gBattleMons[battlerDef].status1 & STATUS1_ANY || GetBattlerAbility(battlerDef) == ABILITY_COMATOSE)
            basePower *= 2;
        break;
    case EFFECT_ASSURANCE:
        if (gProtectStructs[battlerDef].physicalDmg != 0 || gProtectStructs[battlerDef].specialDmg != 0 || gProtectStructs[battlerDef].confusionSelfDmg != 0)
            basePower *= 2;
        break;
    case EFFECT_TRUMP_CARD:
        i = GetBattleMonMoveSlot(&gBattleMons[battlerAtk], move);
        if (i != 4)
        {
            if (gBattleMons[battlerAtk].pp[i] >= ARRAY_COUNT(sTrumpCardPowerTable))
                basePower = sTrumpCardPowerTable[ARRAY_COUNT(sTrumpCardPowerTable) - 1];
            else
                basePower = sTrumpCardPowerTable[i];
        }
        break;
    case EFFECT_ACROBATICS:
        if (gBattleMons[battlerAtk].item == ITEM_NONE
            // Edge case, because removal of items happens after damage calculation.
            || (gSpecialStatuses[battlerAtk].gemBoost && GetBattlerHoldEffect(battlerAtk, FALSE) == HOLD_EFFECT_GEMS))
            basePower *= 2;
        break;
    case EFFECT_LOW_KICK:
        weight = GetBattlerWeight(battlerDef);
        for (i = 0; sWeightToDamageTable[i] != 0xFFFF; i += 2)
        {
            if (sWeightToDamageTable[i] > weight)
                break;
        }
        if (sWeightToDamageTable[i] != 0xFFFF)
            basePower = sWeightToDamageTable[i + 1];
        else
            basePower = 120;
        break;
    case EFFECT_HEAT_CRASH:
        weight = GetBattlerWeight(battlerAtk) / GetBattlerWeight(battlerDef);
        
        if (weight >= ARRAY_COUNT(sHeatCrushPowerTable))
            basePower = sHeatCrushPowerTable[ARRAY_COUNT(sHeatCrushPowerTable) - 1];
        else
            basePower = sHeatCrushPowerTable[weight];
        break;
    case EFFECT_PUNISHMENT:
        basePower = 60 + (CountBattlerStatIncreases(battlerDef, FALSE) * 20);
        if (basePower > 200)
            basePower = 200;
        break;
    case EFFECT_STORED_POWER:
        basePower += (CountBattlerStatIncreases(battlerAtk, TRUE) * 20);
        break;
    case EFFECT_ELECTRO_BALL:
		if(gBattleMons[battlerAtk].species != SPECIES_ELECTRODE){
        speed = GetBattlerTotalSpeedStat(battlerAtk) / GetBattlerTotalSpeedStat(battlerDef);
        if (speed >= ARRAY_COUNT(sSpeedDiffPowerTable))
            speed = ARRAY_COUNT(sSpeedDiffPowerTable) - 1;
        basePower = sSpeedDiffPowerTable[speed];
		}else{
			basePower = 80;
		}
        break;
    case EFFECT_GYRO_BALL:
        basePower = ((25 * GetBattlerTotalSpeedStat(battlerDef)) / GetBattlerTotalSpeedStat(battlerAtk)) + 1;
        if (basePower > 150)
            basePower = 150;
        break;
    case EFFECT_ECHOED_VOICE:
        if (gFieldTimers.echoVoiceCounter != 0)
        {
            if (gFieldTimers.echoVoiceCounter >= 5)
                basePower *= 5;
            else
                basePower *= gFieldTimers.echoVoiceCounter;
        }
        break;
    case EFFECT_PAYBACK:
        if (GetBattlerTurnOrderNum(battlerAtk) > GetBattlerTurnOrderNum(battlerDef)
            && (gDisableStructs[battlerDef].isFirstTurn != 2 || B_PAYBACK_SWITCH_BOOST < GEN_5))
            basePower *= 2;
        break;
    case EFFECT_ROUND:
        if (gChosenMoveByBattler[BATTLE_PARTNER(battlerAtk)] == MOVE_ROUND && !(gAbsentBattlerFlags & gBitTable[BATTLE_PARTNER(battlerAtk)]))
            basePower *= 2;
        break;
    case EFFECT_FUSION_COMBO:
        if (gBattleMoves[gLastUsedMove].effect == EFFECT_FUSION_COMBO && move != gLastUsedMove)
            basePower *= 2;
        break;
    case EFFECT_EXPANDING_FORCE:
        if (gFieldStatuses & STATUS_FIELD_PSYCHIC_TERRAIN && IsBattlerGrounded(battlerAtk))
            basePower = 120;
        break;
    case EFFECT_BOLT_BEAK:
        if (GetBattlerTurnOrderNum(battlerAtk) < GetBattlerTurnOrderNum(battlerDef))
            basePower *= 2;
        break;
    case EFFECT_RISING_VOLTAGE:
		if(gFieldStatuses & STATUS_FIELD_ELECTRIC_TERRAIN && IsBattlerGrounded(battlerAtk))
            basePower *= 2;
		break;
    }

    //For Signature Moves
    if(getMoveBasePower(move, speciesId, GetBattlerAbility(battlerAtk)) > basePower)
        basePower = getMoveBasePower(move, speciesId, GetBattlerAbility(battlerAtk));

    //For Special Cases
    switch(speciesId){
        case SPECIES_TYPHLOSION:
            if(60 > basePower)
                basePower = 60;
        break;
    }
	
	if(basePower > powerLimit)
		basePower = powerLimit;

    if (basePower == 0)
        basePower = 1;
    return basePower;
}

// Supreme Overlord adds a damage boost for each fainted ally.
// The first ally adds a x1.2 boost, and subsequent allies add an extra x0.1 boost each.
static u16 GetSupremeOverlordModifier(u8 battlerId)
{
    u8 side = GetBattlerSide(battlerId);
    struct Pokemon *party = (side == B_SIDE_PLAYER) ? gPlayerParty : gEnemyParty;
    u8 i;

    u16 modifier = UQ_4_12(1.0);
    bool8 appliedFirstBoost = FALSE;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (GetMonData(&party[i], MON_DATA_HP) == 0)
 modifier += (!appliedFirstBoost) ? UQ_4_12(0.2) : UQ_4_12(0.1);
        appliedFirstBoost = TRUE;
    }

    return modifier;
}

static u32 CalcMoveBasePowerAfterModifiers(u16 move, u8 battlerAtk, u8 battlerDef, u8 moveType, bool32 updateFlags)
{
    u32 i, ability;
    u32 holdEffectAtk, holdEffectParamAtk;
    u16 basePower = CalcMoveBasePower(move, battlerAtk, battlerDef);
    u16 holdEffectModifier;
    u16 modifier = UQ_4_12(1.0);

    // attacker's abilities
    switch (GetBattlerAbility(battlerAtk))
    {
    case ABILITY_TECHNICIAN:
        if (basePower <= 60)
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case ABILITY_EARLY_EVOLVER:
        if (basePower <= 40 && gBattleMons[battlerAtk].level < 50)
            MulModifier(&modifier, UQ_4_12(1.5));
        if (basePower <= 40 && gBattleMons[battlerAtk].level >= 50)
            MulModifier(&modifier, UQ_4_12(2.0));
        else if (basePower <= 60 && gBattleMons[battlerAtk].level > 30)
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case ABILITY_FLARE_BOOST:
        if (gBattleMons[battlerAtk].status1 & STATUS1_BURN && IS_BATTLER_MOVE_SPECIAL(move, battlerAtk))
           MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case ABILITY_TOXIC_BOOST:
        if (gBattleMons[battlerAtk].status1 & STATUS1_PSN_ANY && IS_BATTLER_MOVE_PHYSICAL(move, battlerAtk))
           MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case ABILITY_RECKLESS:
        if (gBattleMoves[move].flags & FLAG_RECKLESS_BOOST)
           MulModifier(&modifier, UQ_4_12(1.2));
        break;
    case ABILITY_IRON_FIST:
        if (gBattleMoves[move].flags & FLAG_IRON_FIST_BOOST)
           MulModifier(&modifier, UQ_4_12(1.3));
        break;
	case ABILITY_STRIKER:
        if (gBattleMoves[move].flags & FLAG_STRIKER_BOOST)
           MulModifier(&modifier, UQ_4_12(1.3));
        break;
	case ABILITY_SHARPNESS:
        if (gBattleMoves[move].flags & FLAG_BLADEMASTER_BOOST)
           MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case ABILITY_SHEER_FORCE:
        if (gBattleMoves[move].flags & FLAG_SHEER_FORCE_BOOST)
           MulModifier(&modifier, UQ_4_12(1.3));
        break;
    case ABILITY_SAND_FORCE:
        if (moveType == TYPE_STEEL || moveType == TYPE_ROCK || moveType == TYPE_GROUND)
           MulModifier(&modifier, UQ_4_12(1.3));
        break;
    case ABILITY_RIVALRY:
        if (GetGenderFromSpeciesAndPersonality(gBattleMons[battlerAtk].species, gBattleMons[battlerAtk].personality) != MON_GENDERLESS
            && GetGenderFromSpeciesAndPersonality(gBattleMons[battlerDef].species, gBattleMons[battlerDef].personality) != MON_GENDERLESS)
        {
            if (GetGenderFromSpeciesAndPersonality(gBattleMons[battlerAtk].species, gBattleMons[battlerAtk].personality)
             == GetGenderFromSpeciesAndPersonality(gBattleMons[battlerDef].species, gBattleMons[battlerDef].personality))
               MulModifier(&modifier, UQ_4_12(1.25));
            else
               MulModifier(&modifier, UQ_4_12(0.75));
        }
        break;
    case ABILITY_ANALYTIC:
        if (GetBattlerTurnOrderNum(battlerAtk) == gBattlersCount - 1 && move != MOVE_FUTURE_SIGHT && move != MOVE_DOOM_DESIRE)
           MulModifier(&modifier, UQ_4_12(1.3));
        break;
    case ABILITY_TOUGH_CLAWS:
        if (gBattleMoves[move].flags & FLAG_MAKES_CONTACT)
           MulModifier(&modifier, UQ_4_12(1.3));
        break;
    case ABILITY_STRONG_JAW:
        if (gBattleMoves[move].flags & FLAG_STRONG_JAW_BOOST)
           MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case ABILITY_MEGA_LAUNCHER:
        if (gBattleMoves[move].flags & FLAG_MEGA_LAUNCHER_BOOST)
           MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case ABILITY_WATER_BUBBLE:
        if (moveType == TYPE_WATER)
           MulModifier(&modifier, UQ_4_12(2.0));
        break;
    case ABILITY_STEELWORKER:
        if (moveType == TYPE_STEEL)
           MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case ABILITY_STEELY_SPIRIT:
        if (moveType == TYPE_STEEL)
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case ABILITY_TRANSISTOR:
        if (moveType == TYPE_ELECTRIC)
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case ABILITY_DRAGONS_MAW:
        if (moveType == TYPE_DRAGON)
            MulModifier(&modifier, UQ_4_12(1.3));
        break;
	case ABILITY_JUSTICE_FISTS:
        if (gBattleMoves[move].flags & FLAG_IRON_FIST_BOOST && moveType != TYPE_FIGHTING)
           MulModifier(&modifier, UQ_4_12(1.3));
	    else if (moveType == TYPE_FIGHTING)
           MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case ABILITY_PIXILATE:
        if (moveType == TYPE_FAIRY && gBattleStruct->ateBoost[battlerAtk])
            MulModifier(&modifier, UQ_4_12(1.3));
        break;
    case ABILITY_GALVANIZE:
        if (moveType == TYPE_ELECTRIC && gBattleStruct->ateBoost[battlerAtk])
            MulModifier(&modifier, UQ_4_12(1.3));
        break;
    case ABILITY_REFRIGERATE:
        if (moveType == TYPE_ICE && gBattleStruct->ateBoost[battlerAtk])
            MulModifier(&modifier, UQ_4_12(1.3));
        break;
    case ABILITY_AERILATE:
        if (moveType == TYPE_FLYING && gBattleStruct->ateBoost[battlerAtk])
            MulModifier(&modifier, UQ_4_12(1.3));
        break;
    case ABILITY_NORMALIZE:
        if (moveType == TYPE_NORMAL && gBattleStruct->ateBoost[battlerAtk])
            MulModifier(&modifier, UQ_4_12(1.3));
        break;
    case ABILITY_LIQUID_VOICE:
	    if (gBattleMoves[move].flags & FLAG_SOUND)
		MulModifier(&modifier, UQ_4_12(1.2));
	    break;
	case ABILITY_GORILLA_TACTICS:
        if (IS_BATTLER_MOVE_PHYSICAL(move, battlerAtk))
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
	case ABILITY_SAGE_POWER:
        if (IS_BATTLER_MOVE_SPECIAL(move, battlerAtk))
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
	case ABILITY_CACOPHONY:
		if (gBattleMoves[move].flags & FLAG_SOUND)
			MulModifier(&modifier, UQ_4_12(1.3));
        break;
    case ABILITY_POWER_SPOT:
        MulModifier(&modifier, UQ_4_12(1.3));
        break;
    case ABILITY_PUNK_ROCK:
        if (gBattleMoves[move].flags & FLAG_SOUND)
            MulModifier(&modifier, UQ_4_12(1.3));
        break;
    case ABILITY_SUPREME_OVERLORD:
        MulModifier(&modifier, GetSupremeOverlordModifier(battlerAtk));
        break;
    }

    // field abilities
    if ((IsAbilityOnField(ABILITY_DARK_AURA) && moveType == TYPE_DARK)
        || (IsAbilityOnField(ABILITY_FAIRY_AURA) && moveType == TYPE_FAIRY))
    {
        if (IsAbilityOnField(ABILITY_AURA_BREAK))
            MulModifier(&modifier, UQ_4_12(0.75));
        else
            MulModifier(&modifier, UQ_4_12(1.25));
    }

    // attacker partner's abilities
    if (IsBattlerAlive(BATTLE_PARTNER(battlerAtk)))
    {
        switch (GetBattlerAbility(BATTLE_PARTNER(battlerAtk)))
        {
        case ABILITY_POWER_SPOT:
            MulModifier(&modifier, UQ_4_12(1.3));
            break;
        case ABILITY_STEELY_SPIRIT:
            if (moveType == TYPE_STEEL)
                MulModifier(&modifier, UQ_4_12(1.5));
            break;
        }
    }

    // target's abilities
    ability = GetBattlerAbility(battlerDef);
    switch (ability)
    {
    case ABILITY_HEATPROOF:
    case ABILITY_WATER_BUBBLE:
        if (moveType == TYPE_FIRE)
        {
            MulModifier(&modifier, UQ_4_12(0.5));
            if (updateFlags)
                RecordAbilityBattle(battlerDef, ability);
        }
        break;
    case ABILITY_DRY_SKIN:
        if (moveType == TYPE_FIRE)
            MulModifier(&modifier, UQ_4_12(1.25));
        break;
    case ABILITY_FLUFFY:
        if (IsMoveMakingContact(move, battlerAtk))
        {
            MulModifier(&modifier, UQ_4_12(0.5));
            if (updateFlags)
                RecordAbilityBattle(battlerDef, ability);
        }
        if (moveType == TYPE_FIRE)
            MulModifier(&modifier, UQ_4_12(2.0));
        break;
    case ABILITY_JUSTIFIED:
        if (moveType == TYPE_DARK )
        {
            MulModifier(&modifier, UQ_4_12(0.5));
            if (updateFlags)
                RecordAbilityBattle(battlerDef, ability);
        }
        break;
	case ABILITY_SLIMY:
        if (IsMoveMakingContact(move, battlerAtk))
        {
            MulModifier(&modifier, UQ_4_12(0.5));
            if (updateFlags)
                RecordAbilityBattle(battlerDef, ability);
        }
        if (moveType == TYPE_WATER)
            MulModifier(&modifier, UQ_4_12(2.0));
        break;
    case ABILITY_DAMP:
        if (moveType == TYPE_FIRE)
            MulModifier(&modifier, UQ_4_12(0.50));
        break;
    }

    holdEffectAtk = GetBattlerHoldEffect(battlerAtk, TRUE);
    holdEffectParamAtk = GetBattlerHoldEffectParam(battlerAtk);
    if (holdEffectParamAtk > 100)
        holdEffectParamAtk = 100;

    holdEffectModifier = UQ_4_12(1.0) + sPercentToModifier[holdEffectParamAtk];

    // attacker's hold effect
    switch (holdEffectAtk)
    {
    case HOLD_EFFECT_MUSCLE_BAND:
        if (IS_BATTLER_MOVE_PHYSICAL(move, battlerAtk))
            MulModifier(&modifier, holdEffectModifier);
        break;
    case HOLD_EFFECT_WISE_GLASSES:
        if (IS_BATTLER_MOVE_SPECIAL(move, battlerAtk))
            MulModifier(&modifier, holdEffectModifier);
        break;
    case HOLD_EFFECT_LUSTROUS_ORB:
        if (gBattleMons[battlerAtk].species == SPECIES_PALKIA && (moveType == TYPE_WATER || moveType == TYPE_DRAGON))
            MulModifier(&modifier, holdEffectModifier);
        break;
    case HOLD_EFFECT_ADAMANT_ORB:
        if (gBattleMons[battlerAtk].species == SPECIES_DIALGA && (moveType == TYPE_STEEL || moveType == TYPE_DRAGON))
            MulModifier(&modifier, holdEffectModifier);
        break;
    case HOLD_EFFECT_GRISEOUS_ORB:
        if (gBattleMons[battlerAtk].species == SPECIES_GIRATINA && (moveType == TYPE_GHOST || moveType == TYPE_DRAGON))
            MulModifier(&modifier, holdEffectModifier);
        break;
    case HOLD_EFFECT_SOUL_DEW:
        if ((gBattleMons[battlerAtk].species == SPECIES_LATIAS || gBattleMons[battlerAtk].species == SPECIES_LATIOS))
            MulModifier(&modifier, holdEffectModifier);
        break;
    case HOLD_EFFECT_GEMS:
        if (gSpecialStatuses[battlerAtk].gemBoost && gBattleMons[battlerAtk].item)
            MulModifier(&modifier, UQ_4_12(1.0) + sPercentToModifier[gSpecialStatuses[battlerAtk].gemParam]);
        break;
    case HOLD_EFFECT_BUG_POWER:
    case HOLD_EFFECT_STEEL_POWER:
    case HOLD_EFFECT_GROUND_POWER:
    case HOLD_EFFECT_ROCK_POWER:
    case HOLD_EFFECT_GRASS_POWER:
    case HOLD_EFFECT_DARK_POWER:
    case HOLD_EFFECT_FIGHTING_POWER:
    case HOLD_EFFECT_ELECTRIC_POWER:
    case HOLD_EFFECT_WATER_POWER:
    case HOLD_EFFECT_FLYING_POWER:
    case HOLD_EFFECT_POISON_POWER:
    case HOLD_EFFECT_ICE_POWER:
    case HOLD_EFFECT_GHOST_POWER:
    case HOLD_EFFECT_PSYCHIC_POWER:
    case HOLD_EFFECT_FIRE_POWER:
    case HOLD_EFFECT_DRAGON_POWER:
    case HOLD_EFFECT_NORMAL_POWER:
    case HOLD_EFFECT_FAIRY_POWER:
        for (i = 0; i < ARRAY_COUNT(sHoldEffectToType); i++)
        {
            if (holdEffectAtk == sHoldEffectToType[i][0])
            {
                if (moveType == sHoldEffectToType[i][1])
                    MulModifier(&modifier, holdEffectModifier);
                break;
            }
        }
        break;
    case HOLD_EFFECT_PLATE:
        if (moveType == ItemId_GetSecondaryId(gBattleMons[battlerAtk].item))
            MulModifier(&modifier, holdEffectModifier);
        break;
    case HOLD_EFFECT_MEMORY:
        if (moveType == ItemId_GetSecondaryId(gBattleMons[battlerAtk].item))
            MulModifier(&modifier, holdEffectModifier);
        break;
    }

    // move effect
    switch (gBattleMoves[move].effect)
    {
    case EFFECT_FACADE:
        if (gBattleMons[battlerAtk].status1 & (STATUS1_BURN | STATUS1_PSN_ANY | STATUS1_PARALYSIS))
            MulModifier(&modifier, UQ_4_12(2.0));
        break;
    case EFFECT_BRINE:
        if (gBattleMons[battlerDef].hp <= (gBattleMons[battlerDef].maxHP / 2))
            MulModifier(&modifier, UQ_4_12(2.0));
        break;
    case EFFECT_VENOSHOCK:
        if (gBattleMons[battlerAtk].status1 & STATUS1_PSN_ANY)
            MulModifier(&modifier, UQ_4_12(2.0));
        break;
    case EFFECT_RETALITATE:
        // todo
        break;
    case EFFECT_SOLARBEAM:
        if (WEATHER_HAS_EFFECT && gBattleWeather & (WEATHER_HAIL_ANY | WEATHER_SANDSTORM_ANY | WEATHER_RAIN_ANY))
            MulModifier(&modifier, UQ_4_12(0.5));
        break;
    case EFFECT_STOMPING_TANTRUM:
        if (gBattleStruct->lastMoveFailed & gBitTable[battlerAtk])
            MulModifier(&modifier, UQ_4_12(2.0));
        break;
    case EFFECT_BULLDOZE:
    case EFFECT_MAGNITUDE:
    case EFFECT_EARTHQUAKE:
        if (gFieldStatuses & STATUS_FIELD_GRASSY_TERRAIN && !(gStatuses3[battlerDef] & STATUS3_SEMI_INVULNERABLE))
            MulModifier(&modifier, UQ_4_12(0.5));
        break;
    case EFFECT_KNOCK_OFF:
        if (gBattleMons[battlerDef].item != ITEM_NONE && GetBattlerAbility(battlerDef) != ABILITY_STICKY_HOLD)
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case EFFECT_SCORCHING_SANDS:
        if (WEATHER_HAS_EFFECT && gBattleWeather & WEATHER_SANDSTORM_ANY)
            MulModifier(&modifier, UQ_4_12(1.5));
	    break;
    }

    // various effecs
    if (gProtectStructs[battlerAtk].helpingHand)
        MulModifier(&modifier, UQ_4_12(1.5));
    if (gStatuses3[battlerAtk] & STATUS3_CHARGED_UP && moveType == TYPE_ELECTRIC)
        MulModifier(&modifier, UQ_4_12(2.0));
    if (gStatuses3[battlerAtk] & STATUS3_ME_FIRST)
        MulModifier(&modifier, UQ_4_12(1.5));
    if (gFieldStatuses & STATUS_FIELD_GRASSY_TERRAIN && moveType == TYPE_GRASS && IsBattlerGrounded(battlerAtk) && !(gStatuses3[battlerAtk] & STATUS3_SEMI_INVULNERABLE))
        MulModifier(&modifier, (B_TERRAIN_TYPE_BOOST >= GEN_8) ? UQ_4_12(1.5) : UQ_4_12(1.5));
    if (gFieldStatuses & STATUS_FIELD_MISTY_TERRAIN && moveType == TYPE_DRAGON && IsBattlerGrounded(battlerDef) && !(gStatuses3[battlerDef] & STATUS3_SEMI_INVULNERABLE))
        MulModifier(&modifier, UQ_4_12(0.5));
    if (gFieldStatuses & STATUS_FIELD_ELECTRIC_TERRAIN && moveType == TYPE_ELECTRIC && IsBattlerGrounded(battlerAtk) && !(gStatuses3[battlerAtk] & STATUS3_SEMI_INVULNERABLE))
        MulModifier(&modifier, (B_TERRAIN_TYPE_BOOST >= GEN_8) ? UQ_4_12(1.5) : UQ_4_12(1.5));
    if (gFieldStatuses & STATUS_FIELD_PSYCHIC_TERRAIN && moveType == TYPE_PSYCHIC && IsBattlerGrounded(battlerAtk) && !(gStatuses3[battlerAtk] & STATUS3_SEMI_INVULNERABLE))
        MulModifier(&modifier, (B_TERRAIN_TYPE_BOOST >= GEN_8) ? UQ_4_12(1.5) : UQ_4_12(1.5));

    return ApplyModifier(modifier, basePower);
}

static u32 CalcAttackStat(u16 move, u8 battlerAtk, u8 battlerDef, u8 moveType, bool32 isCrit, bool32 updateFlags)
{
    u8 i;
	u8 atkStage;
    u32 atkStat;
    u16 modifier;
	u8 numStatChanges = 0;
    u8 stat = getAttackingStat(move, battlerAtk);

    if (gBattleMoves[move].effect == EFFECT_FOUL_PLAY)
    {
        if (IS_BATTLER_MOVE_PHYSICAL(move, battlerAtk))
        {
            atkStat = gBattleMons[battlerDef].attack;
            atkStage = gBattleMons[battlerDef].statStages[STAT_ATK];
        }
        else
        {
            atkStat = gBattleMons[battlerDef].spAttack;
            atkStage = gBattleMons[battlerDef].statStages[STAT_SPATK];
        }
    }
	else if (gBattleMoves[move].effect == EFFECT_BODY_PRESS)
    {
        atkStat = gBattleMons[battlerAtk].defense;
        atkStage = gBattleMons[battlerAtk].statStages[STAT_DEF];
    }
    else
    {
        switch(stat)
        {
            case STAT_ATK:
                atkStat = gBattleMons[battlerAtk].attack;
                atkStage = gBattleMons[battlerAtk].statStages[STAT_ATK];
                break;
            case STAT_DEF:
                atkStat = gBattleMons[battlerAtk].defense;
                atkStage = gBattleMons[battlerAtk].statStages[STAT_DEF];
                break;
            case STAT_SPATK:
                atkStat = gBattleMons[battlerAtk].spAttack;
                atkStage = gBattleMons[battlerAtk].statStages[STAT_SPATK];
                break;
            case STAT_SPDEF:
                atkStat = gBattleMons[battlerAtk].spDefense;
                atkStage = gBattleMons[battlerAtk].statStages[STAT_SPDEF];
                break;
            case STAT_SPEED:
                atkStat = gBattleMons[battlerAtk].speed;
                atkStage = gBattleMons[battlerAtk].statStages[STAT_SPEED];
                break;
        }
    }
	
    // critical hits ignore attack stat's stage drops
    if (isCrit && atkStage < 6)
        atkStage = 6;
    // pokemon with unaware ignore attack stat changes while taking damage
    if (GetBattlerAbility(battlerDef) == ABILITY_UNAWARE)
        atkStage = 6;

    atkStat *= gStatStageRatios[atkStage][0];
    atkStat /= gStatStageRatios[atkStage][1];

    // apply attack stat modifiers
    modifier = UQ_4_12(1.0);

    // attacker's abilities
    switch (GetBattlerAbility(battlerAtk))
    {
    case ABILITY_HUGE_POWER:
    case ABILITY_PURE_POWER:
        if (IS_BATTLER_MOVE_PHYSICAL(move, battlerAtk) && !(FlagGet(FLAG_LEVELESS_MODE) && FlagGet(FLAG_NO_EVOLUTION_MODE)))
            MulModifier(&modifier, UQ_4_12(2.0));
        break;
	case ABILITY_FELINE_PROWESS:
        if (IS_BATTLER_MOVE_SPECIAL(move, battlerAtk) && !(FlagGet(FLAG_LEVELESS_MODE) && FlagGet(FLAG_NO_EVOLUTION_MODE)))
            MulModifier(&modifier, UQ_4_12(2.0));
        break;
    case ABILITY_SLOW_START:
        if (gDisableStructs[battlerAtk].slowStartTimer != 0)
            MulModifier(&modifier, UQ_4_12(0.5));
        break;
    case ABILITY_SOLAR_POWER:
        if (IS_BATTLER_MOVE_SPECIAL(move, battlerAtk) && WEATHER_HAS_EFFECT && gBattleWeather & WEATHER_SUN_ANY)
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case ABILITY_DEFEATIST:
        if (gBattleMons[battlerAtk].hp <= (gBattleMons[battlerDef].maxHP / 2))
            MulModifier(&modifier, UQ_4_12(0.5));
        break;
    case ABILITY_FLASH_FIRE:
        if (moveType == TYPE_FIRE && gBattleResources->flags->flags[battlerAtk] & RESOURCE_FLAG_FLASH_FIRE)
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case ABILITY_SWARM:
        if (moveType == TYPE_BUG)
            MulModifier(&modifier, UQ_4_12(1.3));
        break;
    case ABILITY_TORRENT:
        if (moveType == TYPE_WATER)
            MulModifier(&modifier, UQ_4_12(1.3));
        break;
    case ABILITY_BLAZE:
        if (moveType == TYPE_FIRE)
            MulModifier(&modifier, UQ_4_12(1.3));
        break;
    case ABILITY_OVERGROW:
        if (moveType == TYPE_GRASS)
            MulModifier(&modifier, UQ_4_12(1.3));
        break;
    case ABILITY_PLUS:
    case ABILITY_MINUS:
        if (IsBattlerAlive(BATTLE_PARTNER(battlerAtk)))
        {
            u32 partnerAbility = GetBattlerAbility(BATTLE_PARTNER(battlerAtk));
            if (partnerAbility == ABILITY_PLUS || partnerAbility == ABILITY_MINUS)
                MulModifier(&modifier, UQ_4_12(1.5));
        }
        else if (moveType == TYPE_ELECTRIC)
            MulModifier(&modifier, UQ_4_12(1.3));
        break;
    case ABILITY_FLOWER_GIFT:
        if (gBattleMons[battlerAtk].species == SPECIES_CHERRIM && WEATHER_HAS_EFFECT && gBattleWeather & WEATHER_SUN_ANY)
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case ABILITY_HUSTLE:
        if (IS_BATTLER_MOVE_PHYSICAL(move, battlerAtk))
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case ABILITY_STAKEOUT:
        if (gDisableStructs[battlerDef].isFirstTurn == 2) // just switched in
            MulModifier(&modifier, UQ_4_12(2.0));
        break;
    case ABILITY_GUTS:
        if (gBattleMons[battlerAtk].status1 & STATUS1_ANY && IS_BATTLER_MOVE_PHYSICAL(move, battlerAtk))
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
    }

    // target's abilities
    switch (GetBattlerAbility(battlerDef))
    {
    case ABILITY_THICK_FAT:
        if (moveType == TYPE_FIRE || moveType == TYPE_ICE)
        {
            MulModifier(&modifier, UQ_4_12(0.5));
            if (updateFlags)
                RecordAbilityBattle(battlerDef, ABILITY_THICK_FAT);
        }
        break;
	case ABILITY_MAGMA_ARMOR:
        if (moveType == TYPE_WATER || moveType == TYPE_GROUND)
        {
            MulModifier(&modifier, UQ_4_12(0.5));
            if (updateFlags)
                RecordAbilityBattle(battlerDef, ABILITY_MAGMA_ARMOR);
        }
        break;
    }

    // ally's abilities
    if (IsBattlerAlive(BATTLE_PARTNER(battlerAtk)))
    {
        switch (GetBattlerAbility(BATTLE_PARTNER(battlerAtk)))
        {
        case ABILITY_FLOWER_GIFT:
            if (gBattleMons[BATTLE_PARTNER(battlerAtk)].species == SPECIES_CHERRIM)
                MulModifier(&modifier, UQ_4_12(1.5));
            break;
        }
    }

    // attacker's hold effect
    switch (GetBattlerHoldEffect(battlerAtk, TRUE))
    {
    case HOLD_EFFECT_THICK_CLUB:
        if ((gBattleMons[battlerAtk].species == SPECIES_CUBONE ||
            gBattleMons[battlerAtk].species == SPECIES_MAROWAK ||
            gBattleMons[battlerAtk].species == SPECIES_MAROWAK_ALOLAN) 
            && IS_BATTLER_MOVE_PHYSICAL(move, battlerAtk))
            MulModifier(&modifier, UQ_4_12(2.0));
        break;
    case HOLD_EFFECT_DEEP_SEA_TOOTH:
        if (gBattleMons[battlerAtk].species == SPECIES_CLAMPERL && 
            IS_BATTLER_MOVE_PHYSICAL(move, battlerAtk) && 
            GetMonData(gBattleMons[battlerAtk], MON_DATA_EXIOLITE_ENABLED, NULL) != 1 &&
            !FlagGet(FLAG_NO_EVOLUTION_MODE))
            MulModifier(&modifier, UQ_4_12(2.0));
        else if ((gBattleMons[battlerAtk].species == SPECIES_HUNTAIL || gBattleMons[battlerAtk].species == SPECIES_CLAMPERL) && IS_BATTLER_MOVE_PHYSICAL(move, battlerAtk) && !FlagGet(FLAG_NO_EVOLUTION_MODE))
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case HOLD_EFFECT_LIGHT_BALL:
        if (gBattleMons[battlerAtk].species == SPECIES_PIKACHU)
            MulModifier(&modifier, UQ_4_12(2.0));
        if ((gBattleMons[battlerAtk].species == SPECIES_RAICHU ||
            gBattleMons[battlerAtk].species == SPECIES_RAICHU_ALOLAN) &&
			 !FlagGet(FLAG_NO_EVOLUTION_MODE))
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case HOLD_EFFECT_CHOICE_BAND:
        if (IS_BATTLER_MOVE_PHYSICAL(move, battlerAtk))
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case HOLD_EFFECT_CHOICE_SPECS:
        if (IS_BATTLER_MOVE_SPECIAL(move, battlerAtk))
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
    }

    return ApplyModifier(modifier, atkStat);
}

static bool32 CanEvolve(u32 species)
{
    u32 i;

    for (i = 0; i < EVOS_PER_MON; i++)
    {
        if (gEvolutionTable[species][i].method && gEvolutionTable[species][i].method != EVO_MEGA_EVOLUTION)
            return TRUE;
    }
    return FALSE;
}

static u32 CalcDefenseStat(u16 move, u8 battlerAtk, u8 battlerDef, u8 moveType, bool32 isCrit, bool32 updateFlags)
{
    u8 i;
	bool32 usesDefStat = FALSE;
	bool32 usesSpDefStat = FALSE;
    u8 defStage;
    u32 defStat, def, spDef;
    u16 modifier;
	u8 numStatChanges = 0;
    u8 defenseStat = STAT_DEF;
	u16 speciesId = GetFormSpeciesId(gBattleMons[battlerAtk].species, gBattleMons[battlerAtk].formId);
    bool8 penetratingMove = FALSE;

    if (gBattleMoves[move].effect == EFFECT_PSYSHOCK || IS_BATTLER_MOVE_PHYSICAL(move, battlerAtk)) // uses defense stat instead of sp.def
    {
        if (gFieldStatuses & STATUS_FIELD_WONDER_ROOM) // the defense stats are swapped
            defenseStat = STAT_SPDEF;
        else
            defenseStat = STAT_DEF;
    }
    else // is special
    {
        if (gFieldStatuses & STATUS_FIELD_WONDER_ROOM) // the defense stats are swapped
            defenseStat = STAT_DEF;
        else
            defenseStat = STAT_SPDEF;
    }

    //Signature Moves
    if(gSignatureMoveList[speciesId].move == move){
        if(gSignatureMoveList[speciesId].modification == SIGNATURE_MOD_DEFENSE_STAT)
            defenseStat = gSignatureMoveList[speciesId].variable;
        else if (gSignatureMoveList[speciesId].modification2 == SIGNATURE_MOD_DEFENSE_STAT)
            defenseStat = gSignatureMoveList[speciesId].variable2;
        else if (gSignatureMoveList[speciesId].modification3 == SIGNATURE_MOD_DEFENSE_STAT)
            defenseStat = gSignatureMoveList[speciesId].variable3;
        else if (gSignatureMoveList[speciesId].modification4 == SIGNATURE_MOD_DEFENSE_STAT)
            defenseStat = gSignatureMoveList[speciesId].variable4;
        else if (gSignatureMoveList[speciesId].modification5 == SIGNATURE_MOD_DEFENSE_STAT)
            defenseStat = gSignatureMoveList[speciesId].variable5;
        else if (gSignatureMoveList[speciesId].modification6 == SIGNATURE_MOD_DEFENSE_STAT)
            defenseStat = gSignatureMoveList[speciesId].variable6;
    }

    switch(defenseStat){
        case STAT_ATK:
            defStat = gBattleMons[battlerDef].attack;
            defStage = gBattleMons[battlerDef].statStages[STAT_ATK];
        break;
        case STAT_DEF:
            defStat = gBattleMons[battlerDef].defense;
            defStage = gBattleMons[battlerDef].statStages[STAT_DEF];
            usesDefStat = TRUE;
        break;
        case STAT_SPATK:
            defStat = gBattleMons[battlerDef].spAttack;
            defStage = gBattleMons[battlerDef].statStages[STAT_SPATK];
        break;
        case STAT_SPDEF:
            defStat = gBattleMons[battlerDef].spDefense;
            defStage = gBattleMons[battlerDef].statStages[STAT_SPDEF];
            usesSpDefStat = TRUE;
        break;
        case STAT_SPEED:
            defStat = gBattleMons[battlerDef].speed;
            defStage = gBattleMons[battlerDef].statStages[STAT_SPEED];
        break;
    }

    //Signature Moves Penetrating
    if(gSignatureMoveList[speciesId].move == move){
        if(gSignatureMoveList[speciesId].modification == SIGNATURE_MOD_PENETRATING)
            penetratingMove = TRUE;
        else if (gSignatureMoveList[speciesId].modification2 == SIGNATURE_MOD_PENETRATING)
            penetratingMove = TRUE;
        else if (gSignatureMoveList[speciesId].modification3 == SIGNATURE_MOD_PENETRATING)
            penetratingMove = TRUE;
        else if (gSignatureMoveList[speciesId].modification4 == SIGNATURE_MOD_PENETRATING)
            penetratingMove = TRUE;
        else if (gSignatureMoveList[speciesId].modification5 == SIGNATURE_MOD_PENETRATING)
            penetratingMove = TRUE;
        else if (gSignatureMoveList[speciesId].modification6 == SIGNATURE_MOD_PENETRATING)
            penetratingMove = TRUE;
    }
	
    // critical hits ignore positive stat changes
    if (isCrit && defStage > 6)
        defStage = 6;
    // pokemon with unaware ignore defense stat changes while dealing damage
    if (GetBattlerAbility(battlerAtk) == ABILITY_UNAWARE)
        defStage = 6;
    // certain moves also ignore stat changes
    if (gBattleMoves[move].flags & FLAG_STAT_STAGES_IGNORED)
        defStage = 6;
    // penetrating hits ignore positive stat changes
    if(penetratingMove && defStage > 6)
        defStage = 6;

    defStat *= gStatStageRatios[defStage][0];
    defStat /= gStatStageRatios[defStage][1];

    // apply defense stat modifiers
    modifier = UQ_4_12(1.0);

    // target's abilities
    switch (GetBattlerAbility(battlerDef))
    {
    case ABILITY_MARVEL_SCALE:
        if (gBattleMons[battlerDef].status1 & STATUS1_ANY && usesDefStat)
        {
            MulModifier(&modifier, UQ_4_12(1.5));
            if (updateFlags)
                RecordAbilityBattle(battlerDef, ABILITY_MARVEL_SCALE);
        }
        break;
    case ABILITY_FUR_COAT:
        if (usesDefStat)
        {
            MulModifier(&modifier, UQ_4_12(2.0));
            if (updateFlags)
                RecordAbilityBattle(battlerDef, ABILITY_FUR_COAT);
        }
        break;
    case ABILITY_BIG_PECKS:
        if (usesDefStat)
        {
            MulModifier(&modifier, UQ_4_12(1.5));
            if (updateFlags)
                RecordAbilityBattle(battlerDef, ABILITY_BIG_PECKS);
        }
        break;
    case ABILITY_GRASS_PELT:
        if (gFieldStatuses & STATUS_FIELD_GRASSY_TERRAIN && usesDefStat)
        {
            MulModifier(&modifier, UQ_4_12(2.0));
            if (updateFlags)
                RecordAbilityBattle(battlerDef, ABILITY_GRASS_PELT);
        }
        else if (usesDefStat)
        {
            MulModifier(&modifier, UQ_4_12(1.5));
            if (updateFlags)
                RecordAbilityBattle(battlerDef, ABILITY_GRASS_PELT);
        }
        break;
    case ABILITY_FLOWER_GIFT:
        if ((gBattleWeather & WEATHER_SUN_ANY) && !usesDefStat)
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case ABILITY_PUNK_ROCK:
        if (gBattleMoves[move].flags & FLAG_SOUND)
            MulModifier(&modifier, UQ_4_12(2.0));
        break;
	case ABILITY_ICE_SCALES:
        if (usesSpDefStat)
        {
            MulModifier(&modifier, UQ_4_12(2.0));
            if (updateFlags)
                RecordAbilityBattle(battlerDef, ABILITY_ICE_SCALES);
        }
        break;
    }

    // ally's abilities
    if (IsBattlerAlive(BATTLE_PARTNER(battlerDef)))
    {
        switch (GetBattlerAbility(BATTLE_PARTNER(battlerDef)))
        {
        case ABILITY_FLOWER_GIFT:
            if (gBattleMons[BATTLE_PARTNER(battlerDef)].species == SPECIES_CHERRIM && !usesDefStat)
                MulModifier(&modifier, UQ_4_12(1.5));
            break;
        }
    }

    // target's hold effects
    switch (GetBattlerHoldEffect(battlerDef, TRUE))
    {
    case HOLD_EFFECT_DEEP_SEA_SCALE:
        if (gBattleMons[battlerDef].species == SPECIES_CLAMPERL && 
            !usesDefStat && 
            GetMonData(gBattleMons[battlerDef], MON_DATA_EXIOLITE_ENABLED, NULL) != 1 &&
            !FlagGet(FLAG_NO_EVOLUTION_MODE))
            MulModifier(&modifier, UQ_4_12(2.0));
        else if ((gBattleMons[battlerDef].species == SPECIES_GOREBYSS || gBattleMons[battlerDef].species == SPECIES_CLAMPERL) && !usesDefStat && !FlagGet(FLAG_NO_EVOLUTION_MODE))
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case HOLD_EFFECT_METAL_POWDER:
        if (gBattleMons[battlerDef].species == SPECIES_DITTO && usesDefStat)
            MulModifier(&modifier, UQ_4_12(1.5));
        else if (gBattleMons[battlerDef].status2 & STATUS2_TRANSFORMED && usesDefStat)
            MulModifier(&modifier, UQ_4_12(1.25));
        break;
    case HOLD_EFFECT_EVIOLITE:
        if ((CanEvolve(gBattleMons[battlerDef].species) || (gBaseStats[gBattleMons[battlerDef].species].flags & F_WORKS_WITH_EVIOLITE)) &&
			!FlagGet(FLAG_NO_EVOLUTION_MODE) &&
			GetMonData(gBattleMons[battlerDef], MON_DATA_EXIOLITE_ENABLED, NULL) != 1)
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case HOLD_EFFECT_ASSAULT_VEST:
        if (!usesDefStat)
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
    case HOLD_EFFECT_HEART_SCALE:
        if ((gBattleMons[battlerDef].species == SPECIES_LUVDISC) && !usesDefStat && !FlagGet(FLAG_NO_EVOLUTION_MODE))
            MulModifier(&modifier, UQ_4_12(1.5));
        else if (gBattleMons[battlerDef].species == SPECIES_ALOMOMOLA && usesDefStat && !FlagGet(FLAG_NO_EVOLUTION_MODE))
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
    }
	
	// target's hold item
    switch (gBattleMons[battlerDef].item)
    {
    case ITEM_ODD_KEYSTONE:
        if (gBattleMons[battlerDef].species == SPECIES_SPIRITOMB && !usesDefStat)
            MulModifier(&modifier, UQ_4_12(1.5));
        break;
    }

    // sandstorm sp.def boost for rock types
    if (IS_BATTLER_OF_TYPE(battlerDef, TYPE_ROCK) && WEATHER_HAS_EFFECT && gBattleWeather & WEATHER_SANDSTORM_ANY && !usesDefStat)
        MulModifier(&modifier, UQ_4_12(1.5));

	 if (IS_BATTLER_OF_TYPE(battlerDef, TYPE_ICE) && WEATHER_HAS_EFFECT && gBattleWeather & WEATHER_HAIL_ANY && !usesDefStat)
        MulModifier(&modifier, UQ_4_12(1.5));

    return ApplyModifier(modifier, defStat);
}

static u32 CalcFinalDmg(u32 dmg, u16 move, u8 battlerAtk, u8 battlerDef, u8 moveType, u16 typeEffectivenessModifier, bool32 isCrit, bool32 updateFlags)
{
    u32 percentBoost;
    u32 abilityAtk = GetBattlerAbility(battlerAtk);
    u32 abilityDef = GetBattlerAbility(battlerDef);
    u32 defSide = GET_BATTLER_SIDE(battlerDef);
    u16 finalModifier = UQ_4_12(1.0);
    u8 attackermonotypeboost = getNumberOfPokemonWithTypesOnParty(battlerAtk, moveType);
    u8 defendermonotypeboost = getNumberOfPokemonWithTypesOnParty(battlerDef, moveType);

    // check multiple targets in double battle
    if (GetMoveTargetCount(move, battlerAtk, battlerDef) >= 2)
        MulModifier(&finalModifier, UQ_4_12(0.75));

    // take type effectiveness
    MulModifier(&finalModifier, typeEffectivenessModifier);

    // check crit
    if (isCrit)
        dmg = ApplyModifier((B_CRIT_MULTIPLIER >= GEN_6 ? UQ_4_12(1.5) : UQ_4_12(2.0)), dmg);

    // check burn
    if (gBattleMons[battlerAtk].status1 & STATUS1_BURN && IS_BATTLER_MOVE_PHYSICAL(move, battlerAtk)
        && gBattleMoves[move].effect != EFFECT_FACADE && abilityAtk != ABILITY_GUTS)
        dmg = ApplyModifier(UQ_4_12(0.5), dmg);

    // check sunny/rain weather
    if (WEATHER_HAS_EFFECT && gBattleWeather & WEATHER_RAIN_ANY)
    {
        if (moveType == TYPE_FIRE)
            dmg = ApplyModifier(UQ_4_12(0.5), dmg);
        else if (moveType == TYPE_WATER)
            dmg = ApplyModifier(UQ_4_12(1.5), dmg);
    }
    else if (WEATHER_HAS_EFFECT && gBattleWeather & WEATHER_SUN_ANY)
    {
        if (moveType == TYPE_FIRE)
            dmg = ApplyModifier(UQ_4_12(1.5), dmg);
        else if (moveType == TYPE_WATER)
            dmg = ApplyModifier(UQ_4_12(0.5), dmg);
    }

    // check stab
    if (IS_BATTLER_OF_TYPE(battlerAtk, moveType) && move != MOVE_STRUGGLE)
    {
        if (abilityAtk == ABILITY_ADAPTABILITY)
            MulModifier(&finalModifier, UQ_4_12(2.0));
        else
            MulModifier(&finalModifier, UQ_4_12(1.5));
        
    }

    // reflect, light screen, aurora veil
    if (((gSideStatuses[defSide] & SIDE_STATUS_REFLECT && IS_BATTLER_MOVE_PHYSICAL(move, battlerAtk))
            || (gSideStatuses[defSide] & SIDE_STATUS_LIGHTSCREEN && IS_BATTLER_MOVE_SPECIAL(move, battlerAtk))
            || (gSideStatuses[defSide] & SIDE_STATUS_AURORA_VEIL))
        && abilityAtk != ABILITY_INFILTRATOR)
    {
        if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
            MulModifier(&finalModifier, UQ_4_12(0.66));
        else
            MulModifier(&finalModifier, UQ_4_12(0.5));
    }

    // attacker's abilities
    switch (abilityAtk)
    {
    case ABILITY_TINTED_LENS:
        if (typeEffectivenessModifier <= UQ_4_12(0.5))
            MulModifier(&finalModifier, UQ_4_12(2.0));
        break;
    case ABILITY_SNIPER:
        if (isCrit)
            MulModifier(&finalModifier, UQ_4_12(1.5));
        break;
    case ABILITY_NEUROFORCE:
        if (typeEffectivenessModifier >= UQ_4_12(2.0))
            MulModifier(&finalModifier, UQ_4_12(1.25));
        break;
    }

    // target's abilities
    switch (abilityDef)
    {
    case ABILITY_MULTISCALE:
    case ABILITY_SHADOW_SHIELD:
        if (BATTLER_MAX_HP(battlerDef))
            MulModifier(&finalModifier, UQ_4_12(0.5));
        break;
    case ABILITY_FILTER:
    case ABILITY_SOLID_ROCK:
    case ABILITY_PRISM_ARMOR:
        if (typeEffectivenessModifier >= UQ_4_12(2.0))
            MulModifier(&finalModifier, UQ_4_12(0.75));
        break;
    }

    // target's ally's abilities
    if (IsBattlerAlive(BATTLE_PARTNER(battlerDef)))
    {
        switch (GetBattlerAbility(BATTLE_PARTNER(battlerDef)))
        {
        case ABILITY_FRIEND_GUARD:
            MulModifier(&finalModifier, UQ_4_12(0.75));
            break;
        }
    }

    // attacker's hold effect
    switch (GetBattlerHoldEffect(battlerAtk, TRUE))
    {
    case HOLD_EFFECT_METRONOME:
        percentBoost = min((gBattleStruct->sameMoveTurns[battlerAtk] * GetBattlerHoldEffectParam(battlerAtk)), 100);
        MulModifier(&finalModifier, UQ_4_12(1.0) + sPercentToModifier[percentBoost]);
        break;
    case HOLD_EFFECT_EXPERT_BELT:
        if (typeEffectivenessModifier >= UQ_4_12(2.0))
            MulModifier(&finalModifier, UQ_4_12(1.2));
        break;
    case HOLD_EFFECT_LIFE_ORB:
        MulModifier(&finalModifier, UQ_4_12(1.3));
        break;
    }

    // target's hold effect
    switch (GetBattlerHoldEffect(battlerDef, TRUE))
    {
    // berries reducing dmg
    case HOLD_EFFECT_RESIST_BERRY:
        if (moveType == GetBattlerHoldEffectParam(battlerDef)
            && (moveType == TYPE_NORMAL || typeEffectivenessModifier >= UQ_4_12(2.0)))
        {
            MulModifier(&finalModifier, UQ_4_12(0.5));
            if (updateFlags)
                gSpecialStatuses[battlerDef].berryReduced = 1;
        }
        break;
    }

    if (gBattleMoves[move].flags & FLAG_DMG_MINIMIZE    && gStatuses3[battlerDef] & STATUS3_MINIMIZED)
        MulModifier(&finalModifier, UQ_4_12(2.0));
    if (gBattleMoves[move].flags & FLAG_DMG_UNDERGROUND && gStatuses3[battlerDef] & STATUS3_UNDERGROUND)
        MulModifier(&finalModifier, UQ_4_12(2.0));
    if (gBattleMoves[move].flags & FLAG_DMG_UNDERWATER  && gStatuses3[battlerDef] & STATUS3_UNDERWATER)
        MulModifier(&finalModifier, UQ_4_12(2.0));
    if (gBattleMoves[move].flags & FLAG_DMG_IN_AIR      && gStatuses3[battlerDef] & STATUS3_ON_AIR)
        MulModifier(&finalModifier, UQ_4_12(2.0));

    dmg = ApplyModifier(finalModifier, dmg);
    if (dmg == 0)
        dmg = 1;

    return dmg;
}

s32 CalculateMoveDamage(u16 move, u8 battlerAtk, u8 battlerDef, u8 moveType, s32 fixedBasePower, bool32 isCrit, bool32 randomFactor, bool32 updateFlags)
{
    s32 dmg;
    u16 typeEffectivenessModifier;

    typeEffectivenessModifier = CalcTypeEffectivenessMultiplier(move, moveType, battlerAtk, battlerDef, updateFlags);

    // Don't calculate damage if the move has no effect on target.
    if (typeEffectivenessModifier == UQ_4_12(0))
        return 0;

    if (fixedBasePower)
        gBattleMovePower = fixedBasePower;
    else
        gBattleMovePower = CalcMoveBasePowerAfterModifiers(move, battlerAtk, battlerDef, moveType, updateFlags);

    // long dmg basic formula
    dmg = ((gBattleMons[battlerAtk].level * 2) / 5) + 2;
    dmg *= gBattleMovePower;
    dmg *= CalcAttackStat(move, battlerAtk, battlerDef, moveType, isCrit, updateFlags);
    dmg /= CalcDefenseStat(move, battlerAtk, battlerDef, moveType, isCrit, updateFlags);
    dmg = (dmg / 50) + 2;

    // Calculate final modifiers.
    dmg = CalcFinalDmg(dmg, move, battlerAtk, battlerDef, moveType, typeEffectivenessModifier, isCrit, updateFlags);

    // Add a random factor.
    if (randomFactor)
    {
        dmg *= 100 - (Random() % 16);
        dmg /= 100;
    }

    if (dmg == 0)
        dmg = 1;

    return dmg;
}

static void MulByTypeEffectiveness(u16 *modifier, u16 move, u8 moveType, u8 battlerDef, u8 defType, u8 battlerAtk, bool32 recordAbilities)
{
    u16 mod = GetTypeModifier(moveType, defType);
    u16 species = gBattleMons[battlerAtk].species;
    u8  formId = gBattleMons[battlerAtk].formId;
    u16 speciesId = GetFormSpeciesId(species, formId);

    if (mod == UQ_4_12(0.0) && GetBattlerHoldEffect(battlerDef, TRUE) == HOLD_EFFECT_RING_TARGET)
    {
        mod = UQ_4_12(1.0);
        if (recordAbilities)
            RecordItemEffectBattle(battlerDef, HOLD_EFFECT_RING_TARGET);
    }
    else if ((moveType == TYPE_FIGHTING || moveType == TYPE_NORMAL) && defType == TYPE_GHOST && gBattleMons[battlerDef].status2 & STATUS2_FORESIGHT && mod == UQ_4_12(0.0))
    {
        mod = UQ_4_12(1.0);
    }
    else if ((moveType == TYPE_FIGHTING || moveType == TYPE_NORMAL) && defType == TYPE_GHOST && GetBattlerAbility(battlerAtk) == ABILITY_SCRAPPY && mod == UQ_4_12(0.0))
    {
        mod = UQ_4_12(1.0);
        if (recordAbilities)
            RecordAbilityBattle(battlerAtk, ABILITY_SCRAPPY);
    }
	else if (moveType == TYPE_POISON && defType == TYPE_STEEL && GetBattlerAbility(battlerAtk) == ABILITY_CORROSION && mod == UQ_4_12(0.0))
    {
        mod = UQ_4_12(1.0);
        if (recordAbilities)
            RecordAbilityBattle(battlerAtk, ABILITY_CORROSION);
    }
	else if (moveType == TYPE_NORMAL && defType == TYPE_GHOST && GetBattlerAbility(battlerAtk) == ABILITY_NORMALIZE && mod == UQ_4_12(0.0))
    {
        mod = UQ_4_12(1.0);
        if (recordAbilities)
            RecordAbilityBattle(battlerAtk, ABILITY_NORMALIZE);
    }
	else if (moveType == TYPE_FIRE && defType == TYPE_ROCK && GetBattlerAbility(battlerAtk) == ABILITY_MOLTEN_DOWN)
    {
        mod = UQ_4_12(2.0); // super-effective
        if (recordAbilities)
            RecordAbilityBattle(battlerAtk, ABILITY_MOLTEN_DOWN);
    }
	else if (moveType == TYPE_ELECTRIC && defType == TYPE_ELECTRIC && GetBattlerAbility(battlerAtk) == ABILITY_OVERCHARGE)
    {
        mod = UQ_4_12(2.0); // super-effective
        if (recordAbilities)
            RecordAbilityBattle(battlerAtk, ABILITY_OVERCHARGE);
    }

    if (moveType == TYPE_PSYCHIC && defType == TYPE_DARK && gStatuses3[battlerDef] & STATUS3_MIRACLE_EYED && mod == UQ_4_12(0.0))
        mod = UQ_4_12(1.0);
    if (gBattleMoves[move].effect == EFFECT_FREEZE_DRY && defType == TYPE_WATER)
        mod = UQ_4_12(2.0);
	if (move == MOVE_CUT && defType == TYPE_GRASS)
        mod = UQ_4_12(2.0);
    if (moveType == TYPE_GROUND && defType == TYPE_FLYING && IsBattlerGrounded(battlerDef) && mod == UQ_4_12(0.0) && !FlagGet(B_FLAG_INVERSE_BATTLE))
        mod = UQ_4_12(1.0);
    //Signature Moves
    if(gSignatureMoveList[speciesId].move == move){
             if(gSignatureMoveList[speciesId].modification == SIGNATURE_MOD_SE_AGAINST_TYPE  && defType == gSignatureMoveList[speciesId].variable)
            mod = UQ_4_12(2.0);
        else if(gSignatureMoveList[speciesId].modification2 == SIGNATURE_MOD_SE_AGAINST_TYPE && defType == gSignatureMoveList[speciesId].variable2)
            mod = UQ_4_12(2.0);
        else if(gSignatureMoveList[speciesId].modification3 == SIGNATURE_MOD_SE_AGAINST_TYPE && defType == gSignatureMoveList[speciesId].variable3)
            mod = UQ_4_12(2.0);
        else if(gSignatureMoveList[speciesId].modification4 == SIGNATURE_MOD_SE_AGAINST_TYPE && defType == gSignatureMoveList[speciesId].variable4)
            mod = UQ_4_12(2.0);
        else if(gSignatureMoveList[speciesId].modification5 == SIGNATURE_MOD_SE_AGAINST_TYPE && defType == gSignatureMoveList[speciesId].variable5)
            mod = UQ_4_12(2.0);
        else if(gSignatureMoveList[speciesId].modification6 == SIGNATURE_MOD_SE_AGAINST_TYPE && defType == gSignatureMoveList[speciesId].variable6)
            mod = UQ_4_12(2.0);
    }

    if (gProtectStructs[battlerDef].kingsShielded && gBattleMoves[move].effect != EFFECT_FEINT)
        mod = UQ_4_12(1.0);

    MulModifier(modifier, mod);
}

static void UpdateMoveResultFlags(u16 modifier)
{
    if (modifier == UQ_4_12(0.0) && !FlagGet(B_FLAG_INVERSE_BATTLE))
    {
        gMoveResultFlags |= MOVE_RESULT_DOESNT_AFFECT_FOE;
        gMoveResultFlags &= ~(MOVE_RESULT_NOT_VERY_EFFECTIVE | MOVE_RESULT_SUPER_EFFECTIVE);
    }
    else if (modifier == UQ_4_12(1.0))
    {
        gMoveResultFlags &= ~(MOVE_RESULT_NOT_VERY_EFFECTIVE | MOVE_RESULT_SUPER_EFFECTIVE | MOVE_RESULT_DOESNT_AFFECT_FOE);
    }
    else if (modifier > UQ_4_12(1.0))
    {
        gMoveResultFlags |= MOVE_RESULT_SUPER_EFFECTIVE;
        gMoveResultFlags &= ~(MOVE_RESULT_NOT_VERY_EFFECTIVE | MOVE_RESULT_DOESNT_AFFECT_FOE);
    }
    else //if (modifier < UQ_4_12(1.0))
    {
        gMoveResultFlags |= MOVE_RESULT_NOT_VERY_EFFECTIVE;
        gMoveResultFlags &= ~(MOVE_RESULT_SUPER_EFFECTIVE | MOVE_RESULT_DOESNT_AFFECT_FOE);
    }
}

static u16 CalcTypeEffectivenessMultiplierInternal(u16 move, u8 moveType, u8 battlerAtk, u8 battlerDef, bool32 recordAbilities, u16 modifier)
{
	MulByTypeEffectiveness(&modifier, move, moveType, battlerDef, gBattleMons[battlerDef].type1, battlerAtk, recordAbilities);
    if (gBattleMons[battlerDef].type2 != gBattleMons[battlerDef].type1)
        MulByTypeEffectiveness(&modifier, move, moveType, battlerDef, gBattleMons[battlerDef].type2, battlerAtk, recordAbilities);
    if (gBattleMons[battlerDef].type3 != TYPE_MYSTERY && gBattleMons[battlerDef].type3 != gBattleMons[battlerDef].type2
        && gBattleMons[battlerDef].type3 != gBattleMons[battlerDef].type1)
        MulByTypeEffectiveness(&modifier, move, moveType, battlerDef, gBattleMons[battlerDef].type3, battlerAtk, recordAbilities);

    if (moveType == TYPE_GROUND && !IsBattlerGrounded(battlerDef))
    {
        modifier = UQ_4_12(0.0);
        if (recordAbilities && GetBattlerAbility(battlerDef) == ABILITY_LEVITATE)
        {
            gLastUsedAbility = ABILITY_LEVITATE;
            gMoveResultFlags |= (MOVE_RESULT_MISSED | MOVE_RESULT_DOESNT_AFFECT_FOE);
            gLastLandedMoves[battlerDef] = 0;
            gBattleCommunication[6] = 4;
            RecordAbilityBattle(battlerDef, ABILITY_LEVITATE);
        }
    }
	
    if (((GetBattlerAbility(battlerDef) == ABILITY_WONDER_GUARD && modifier <= UQ_4_12(1.0))
        || (GetBattlerAbility(battlerDef) == ABILITY_TELEPATHY && battlerDef == BATTLE_PARTNER(battlerAtk)))
        && gBattleMoves[move].power)
    {
        modifier = UQ_4_12(0.0);
        if (recordAbilities)
        {
            gLastUsedAbility = gBattleMons[battlerDef].ability;
            gMoveResultFlags |= MOVE_RESULT_MISSED;
            gLastLandedMoves[battlerDef] = 0;
            gBattleCommunication[6] = 3;
            RecordAbilityBattle(battlerDef, gBattleMons[battlerDef].ability);
        }
    }

    if (GetBattlerAbility(battlerDef) == ABILITY_MOUNTAINEER && moveType == TYPE_ROCK)
    {
        modifier = UQ_4_12(0.0);
        if (recordAbilities)
        {
            gLastUsedAbility = gBattleMons[battlerDef].ability;
            gMoveResultFlags |= (MOVE_RESULT_MISSED | MOVE_RESULT_DOESNT_AFFECT_FOE);
            gLastLandedMoves[battlerDef] = 0;
            gBattleCommunication[6] = 3;
            RecordAbilityBattle(battlerDef, gBattleMons[battlerDef].ability);
        }
    }

    return modifier;
}

u16 CalcTypeEffectivenessMultiplier(u16 move, u8 moveType, u8 battlerAtk, u8 battlerDef, bool32 recordAbilities)
{
    u16 modifier = UQ_4_12(1.0);
    u16 secondtypeModifier = UQ_4_12(1.0);
    u16 species = GetFormSpeciesId(gBattleMons[battlerAtk].species, gBattleMons[battlerAtk].formId);

    if (move != MOVE_STRUGGLE && moveType != TYPE_MYSTERY)
    {
        modifier = CalcTypeEffectivenessMultiplierInternal(move, moveType, battlerAtk, battlerDef, recordAbilities, modifier);
        
        if (gBattleMoves[move].effect == EFFECT_TWO_TYPED_MOVE)
            secondtypeModifier = CalcTypeEffectivenessMultiplierInternal(move, gBattleMoves[move].argument, battlerAtk, battlerDef, recordAbilities, modifier);
        else if(gSignatureMoveList[species].move == move){
            if(gSignatureMoveList[species].modification == SIGNATURE_MOD_SECOND_TYPE)
                secondtypeModifier = CalcTypeEffectivenessMultiplierInternal(move, gSignatureMoveList[species].variable, battlerAtk, battlerDef, recordAbilities, modifier);
            else if(gSignatureMoveList[species].modification2 == SIGNATURE_MOD_SECOND_TYPE)
                secondtypeModifier = CalcTypeEffectivenessMultiplierInternal(move, gSignatureMoveList[species].variable2, battlerAtk, battlerDef, recordAbilities, modifier);
            else if(gSignatureMoveList[species].modification3 == SIGNATURE_MOD_SECOND_TYPE)
                secondtypeModifier = CalcTypeEffectivenessMultiplierInternal(move, gSignatureMoveList[species].variable3, battlerAtk, battlerDef, recordAbilities, modifier);
            else if(gSignatureMoveList[species].modification4 == SIGNATURE_MOD_SECOND_TYPE)
                secondtypeModifier = CalcTypeEffectivenessMultiplierInternal(move, gSignatureMoveList[species].variable4, battlerAtk, battlerDef, recordAbilities, modifier);
            else if(gSignatureMoveList[species].modification5 == SIGNATURE_MOD_SECOND_TYPE)
                secondtypeModifier = CalcTypeEffectivenessMultiplierInternal(move, gSignatureMoveList[species].variable5, battlerAtk, battlerDef, recordAbilities, modifier);
            else if(gSignatureMoveList[species].modification6 == SIGNATURE_MOD_SECOND_TYPE)
                secondtypeModifier = CalcTypeEffectivenessMultiplierInternal(move, gSignatureMoveList[species].variable6, battlerAtk, battlerDef, recordAbilities, modifier);
        }
    }

    if(secondtypeModifier != UQ_4_12(0.0))
        MulModifier(&modifier, secondtypeModifier);
    else
        MulModifier(&modifier, UQ_4_12(0.5));

    if (recordAbilities)
        UpdateMoveResultFlags(modifier);
    return modifier;
}

u16 CalcPartyMonTypeEffectivenessMultiplier(u16 move, u16 speciesDef, u16 abilityDef)
{
    u16 modifier = UQ_4_12(1.0);
    u8 moveType = gBattleMoves[move].type;

    if (move != MOVE_STRUGGLE && moveType != TYPE_MYSTERY)
    {
		if(!FlagGet(FLAG_VANILLA_MODE)){
        MulByTypeEffectiveness(&modifier, move, moveType, 0, gBaseStats[speciesDef].type1, 0, FALSE);
        if (gBaseStats[speciesDef].type2 != gBaseStats[speciesDef].type1)
            MulByTypeEffectiveness(&modifier, move, moveType, 0, gBaseStats[speciesDef].type2, 0, FALSE);

        if (moveType == TYPE_GROUND && abilityDef == ABILITY_LEVITATE && !(gFieldStatuses & STATUS_FIELD_GRAVITY))
            modifier = UQ_4_12(0.0);
        if (abilityDef == ABILITY_WONDER_GUARD && modifier <= UQ_4_12(1.0) && gBattleMoves[move].power)
			modifier = UQ_4_12(0.0);
		if (moveType == TYPE_ROCK && abilityDef == ABILITY_MOUNTAINEER) 
            modifier = UQ_4_12(0.0);
		}else{
        MulByTypeEffectiveness(&modifier, move, moveType, 0, gVanillaBaseStats[speciesDef].type1, 0, FALSE);
        if (gVanillaBaseStats[speciesDef].type2 != gVanillaBaseStats[speciesDef].type1)
            MulByTypeEffectiveness(&modifier, move, moveType, 0, gVanillaBaseStats[speciesDef].type2, 0, FALSE);

        if (moveType == TYPE_GROUND && abilityDef == ABILITY_LEVITATE && !(gFieldStatuses & STATUS_FIELD_GRAVITY))
            modifier = UQ_4_12(0.0);
        if (abilityDef == ABILITY_WONDER_GUARD && modifier <= UQ_4_12(1.0) && gBattleMoves[move].power)
			modifier = UQ_4_12(0.0);
		}
		if (moveType == TYPE_ROCK && abilityDef == ABILITY_MOUNTAINEER) 
            modifier = UQ_4_12(0.0);
    }

    UpdateMoveResultFlags(modifier);
    return modifier;
}

u16 GetTypeModifier(u8 atkType, u8 defType)
{
    if (B_FLAG_INVERSE_BATTLE != 0 && FlagGet(B_FLAG_INVERSE_BATTLE) && !(gFieldStatuses & STATUS_FIELD_INVERSE_ROOM))
        return sInverseTypeEffectivenessTable[atkType][defType];
	else if (gFieldStatuses & STATUS_FIELD_INVERSE_ROOM && !FlagGet(B_FLAG_INVERSE_BATTLE))
		return sInverseTypeEffectivenessTable[atkType][defType];
    else
        return sTypeEffectivenessTable[atkType][defType];
}


u16 GetBattleTypeModifier(u8 atkType, u8 defType, u8 battlerAttacker, u8 battlerDefender, u16 move)
{
    u16 atkAbility = GetBattlerAbility(battlerAttacker);
    u16 defAbility = GetBattlerAbility(battlerDefender);
    
    if (B_FLAG_INVERSE_BATTLE != 0 && FlagGet(B_FLAG_INVERSE_BATTLE) && !(gFieldStatuses & STATUS_FIELD_INVERSE_ROOM))
        return sInverseTypeEffectivenessTable[atkType][defType];
	else if (gFieldStatuses & STATUS_FIELD_INVERSE_ROOM && !FlagGet(B_FLAG_INVERSE_BATTLE))
		return sInverseTypeEffectivenessTable[atkType][defType];
    else
        return sTypeEffectivenessTable[atkType][defType];
}

s32 GetStealthHazardDamage(u8 hazardType, u8 battlerId)
{
    u8 type1 = gBattleMons[battlerId].type1;
    u8 type2 = gBattleMons[battlerId].type2;
    u32 maxHp = gBattleMons[battlerId].maxHP;
    s32 dmg = 0;
    u16 modifier = UQ_4_12(1.0);

    MulModifier(&modifier, GetTypeModifier(hazardType, type1));
    if (type2 != type1)
        MulModifier(&modifier, GetTypeModifier(hazardType, type2));

    switch (modifier)
    {
    case UQ_4_12(0.0):
        dmg = 0;
        break;
    case UQ_4_12(0.25):
        dmg = maxHp / 32;
        if (dmg == 0)
            dmg = 1;
        break;
    case UQ_4_12(0.5):
        dmg = maxHp / 16;
        if (dmg == 0)
            dmg = 1;
        break;
    case UQ_4_12(1.0):
        dmg = maxHp / 8;
        if (dmg == 0)
            dmg = 1;
        break;
    case UQ_4_12(2.0):
        dmg = maxHp / 4;
        if (dmg == 0)
            dmg = 1;
        break;
    case UQ_4_12(4.0):
        dmg = maxHp / 2;
        if (dmg == 0)
            dmg = 1;
        break;
    }

    return dmg;
}

static bool32 IsPartnerMonFromSameTrainer(u8 battlerId)
{
    if (GetBattlerSide(battlerId) == B_SIDE_OPPONENT && gBattleTypeFlags & BATTLE_TYPE_TWO_OPPONENTS)
        return FALSE;
    else if (GetBattlerSide(battlerId) == B_SIDE_PLAYER && gBattleTypeFlags & BATTLE_TYPE_INGAME_PARTNER)
        return FALSE;
    else if (gBattleTypeFlags & BATTLE_TYPE_MULTI)
        return FALSE;
    else
        return TRUE;
}

u16 GetMegaEvolutionSpecies(u16 preEvoSpecies, u16 heldItemId)
{
    u32 i;

    for (i = 0; i < EVOS_PER_MON; i++)
    {
        if (gEvolutionTable[preEvoSpecies][i].method == EVO_MEGA_EVOLUTION
            && gEvolutionTable[preEvoSpecies][i].param == heldItemId){
                return gEvolutionTable[preEvoSpecies][i].targetSpecies;
            }
    }
    return SPECIES_NONE;
}

u16 GetWishMegaEvolutionSpecies(u16 preEvoSpecies, u16 moveId1, u16 moveId2, u16 moveId3, u16 moveId4)
{
    u32 i, par;

    for (i = 0; i < EVOS_PER_MON; i++)
    {
        if (gEvolutionTable[preEvoSpecies][i].method == EVO_MOVE_MEGA_EVOLUTION)
        {
            par = gEvolutionTable[preEvoSpecies][i].param;
            if (par == moveId1 || par == moveId2 || par == moveId3 || par == moveId4){
                return gEvolutionTable[preEvoSpecies][i].targetSpecies;
            }
        }
    }
    return SPECIES_NONE;
}

bool32 CanMegaEvolve(u8 battlerId)
{
    u32 itemId, holdEffect, species;
    struct Pokemon *mon;
    u8 battlerPosition = GetBattlerPosition(battlerId);
    u8 partnerPosition = GetBattlerPosition(BATTLE_PARTNER(battlerId));
    struct MegaEvolutionData *mega = &(((struct ChooseMoveStruct*)(&gBattleResources->bufferA[gActiveBattler][4]))->mega);

    // Check if trainer already mega evolved a pokemon.
    if (mega->alreadyEvolved[battlerPosition] || FlagGet(FLAG_NO_MEGA_MODE))
        return FALSE;
    
    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
    {
        if (IsPartnerMonFromSameTrainer(battlerId)
            && (mega->alreadyEvolved[partnerPosition] || (mega->toEvolve & gBitTable[BATTLE_PARTNER(battlerId)])))
            return FALSE;
    }

    // Gets mon data.
    if (GetBattlerSide(battlerId) == B_SIDE_OPPONENT)
        mon = &gEnemyParty[gBattlerPartyIndexes[battlerId]];
    else
        mon = &gPlayerParty[gBattlerPartyIndexes[battlerId]];

    species = GetMonData(mon, MON_DATA_SPECIES);
    itemId  = GetMonData(mon, MON_DATA_HELD_ITEM);
	
	if(FlagGet(FLAG_MEGA_EVOLVE_WILD_POKEMON) && 
	   GetBattlerSide(gActiveBattler) == B_SIDE_OPPONENT && 
	   !(gBattleTypeFlags & BATTLE_TYPE_TRAINER) &&
	   gBattleMons[gActiveBattler].hp > gBattleMons[gActiveBattler].maxHP / 2){
		return FALSE;
	}

    // Check if there is an entry in the evolution table for regular Mega Evolution.
    if (GetMegaEvolutionSpecies(species, itemId) != SPECIES_NONE && FlagGet(FLAG_RECEIVED_TM04) && !FlagGet(FLAG_NO_EVOLUTION_MODE))
    {
        if (USE_BATTLE_DEBUG && gBattleStruct->debugHoldEffects[battlerId])
            holdEffect = gBattleStruct->debugHoldEffects[battlerId];
        else if (itemId == ITEM_ENIGMA_BERRY)
            holdEffect = gEnigmaBerries[battlerId].holdEffect;
        else
            holdEffect = ItemId_GetHoldEffect(itemId);

        // Can Mega Evolve via Item.
        if (holdEffect == HOLD_EFFECT_MEGA_STONE)
        {
            gBattleStruct->mega.isWishMegaEvo = FALSE;
            return TRUE;
        }
    }

    // Check if there is an entry in the evolution table for Wish Mega Evolution.
    if (GetWishMegaEvolutionSpecies(species, GetMonData(mon, MON_DATA_MOVE1), GetMonData(mon, MON_DATA_MOVE2), GetMonData(mon, MON_DATA_MOVE3), GetMonData(mon, MON_DATA_MOVE4)))
    {
        gBattleStruct->mega.isWishMegaEvo = TRUE;
        return TRUE;
    }

    // No checks passed, the mon CAN'T mega evolve.
    return FALSE;
}

void UndoMegaEvolution(u32 monId)
{
    if (gBattleStruct->mega.evolvedPartyIds[B_SIDE_PLAYER] & gBitTable[monId])
    {
        u8 formId = GetFormIdFromFormSpeciesId(gBattleStruct->mega.playerEvolvedSpecies);
        gBattleStruct->mega.evolvedPartyIds[B_SIDE_PLAYER] &= ~(gBitTable[monId]);
        SetMonData(&gPlayerParty[monId], MON_DATA_SPECIES, &gBattleStruct->mega.playerEvolvedSpecies);
        SetMonData(&gPlayerParty[monId], MON_DATA_FORM_ID, &formId);
        CalculateMonStats(&gPlayerParty[monId]);
    }
    // While not exactly a mega evolution, Zygarde follows the same rules.
    else if (GetMonData(&gPlayerParty[monId], MON_DATA_SPECIES, NULL) == SPECIES_ZYGARDE_COMPLETE)
    {
        SetMonData(&gPlayerParty[monId], MON_DATA_SPECIES, &gBattleStruct->changedSpecies[monId]);
        gBattleStruct->changedSpecies[monId] = 0;
        CalculateMonStats(&gPlayerParty[monId]);
    }
}

void UndoFormChange(u32 monId, u32 side)
{
    u32 i, currSpecies;
    struct Pokemon *party = (side == B_SIDE_PLAYER) ? gPlayerParty : gEnemyParty;
    static const u16 species[][2] = // changed form id, default form id
    {
        {SPECIES_EISCUE_NOICE_FACE, SPECIES_EISCUE},
		{SPECIES_CASTFORM_SUNNY, SPECIES_CASTFORM},
		{SPECIES_CASTFORM_SNOWY, SPECIES_CASTFORM},
		{SPECIES_CASTFORM_RAINY, SPECIES_CASTFORM},
		{SPECIES_CHERRIM_SUNSHINE, SPECIES_CHERRIM},
		{SPECIES_DARMANITAN_ZEN_MODE, SPECIES_DARMANITAN},
		{SPECIES_DARMANITAN_ZEN_MODE_GALARIAN, SPECIES_DARMANITAN_GALARIAN},
		{SPECIES_AEGISLASH_BLADE, SPECIES_AEGISLASH},
        {SPECIES_MIMIKYU_BUSTED, SPECIES_MIMIKYU},
        {SPECIES_DARMANITAN_ZEN_MODE, SPECIES_DARMANITAN},
        {SPECIES_MINIOR, SPECIES_MINIOR_CORE_RED},
        {SPECIES_MINIOR_METEOR_BLUE, SPECIES_MINIOR_CORE_BLUE},
        {SPECIES_MINIOR_METEOR_GREEN, SPECIES_MINIOR_CORE_GREEN},
        {SPECIES_MINIOR_METEOR_INDIGO, SPECIES_MINIOR_CORE_INDIGO},
        {SPECIES_MINIOR_METEOR_ORANGE, SPECIES_MINIOR_CORE_ORANGE},
        {SPECIES_MINIOR_METEOR_VIOLET, SPECIES_MINIOR_CORE_VIOLET},
        {SPECIES_MINIOR_METEOR_YELLOW, SPECIES_MINIOR_CORE_YELLOW},
        {SPECIES_WISHIWASHI_SCHOOL, SPECIES_WISHIWASHI},
		{SPECIES_MORPEKO_HANGRY, SPECIES_MORPEKO},
		{SPECIES_CRAMORANT_GORGING, SPECIES_CRAMORANT},
        {SPECIES_CRAMORANT_GULPING, SPECIES_CRAMORANT},
		{SPECIES_GRENINJA_ASH, SPECIES_GRENINJA_BATTLE_BOND},
    };

    currSpecies = GetMonData(&party[monId], MON_DATA_SPECIES, NULL);
    for (i = 0; i < ARRAY_COUNT(species); i++)
    {
        if (currSpecies == species[i][0])
        {
            SetMonData(&party[monId], MON_DATA_SPECIES, &species[i][1]);
            CalculateMonStats(&party[monId]);
            break;
        }
    }
}

bool32 DoBattlersShareType(u32 battler1, u32 battler2)
{
    s32 i;
    u8 types1[3] = {gBattleMons[battler1].type1, gBattleMons[battler1].type2, gBattleMons[battler1].type3};
    u8 types2[3] = {gBattleMons[battler2].type1, gBattleMons[battler2].type2, gBattleMons[battler2].type3};

    if (types1[2] == TYPE_MYSTERY)
        types1[2] = types1[0];
    if (types2[2] == TYPE_MYSTERY)
        types2[2] = types2[0];

    for (i = 0; i < 3; i++)
    {
        if (types1[i] == types2[0] || types1[i] == types2[1] || types1[i] == types2[2])
            return TRUE;
    }

    return FALSE;
}

bool32 CanBattlerGetOrLoseItem(u8 battlerId, u16 itemId)
{
    u16 species = gBattleMons[battlerId].species;

    if (IS_ITEM_MAIL(itemId))
        return FALSE;
    else if (itemId == ITEM_ENIGMA_BERRY)
        return FALSE;
    else if (species == SPECIES_KYOGRE && itemId == ITEM_BLUE_ORB)
        return FALSE;
    else if (species == SPECIES_GROUDON && itemId == ITEM_RED_ORB)
        return FALSE;
	else if (ItemId_GetHoldEffect(itemId) == HOLD_EFFECT_MEGA_STONE)
        return FALSE;
    // Mega stone cannot be lost if pokemon can mega evolve with it or is already mega evolved.
    else if (ItemId_GetHoldEffect(itemId) == HOLD_EFFECT_MEGA_STONE)
        return FALSE;
    else if (species == SPECIES_GIRATINA && itemId == ITEM_GRISEOUS_ORB)
        return FALSE;
    else if (species == SPECIES_GENESECT && GetBattlerHoldEffect(battlerId, FALSE) == HOLD_EFFECT_DRIVE)
        return FALSE;
    else if (species == SPECIES_SILVALLY && GetBattlerHoldEffect(battlerId, FALSE) == HOLD_EFFECT_MEMORY)
        return FALSE;
    else
        return TRUE;
}

struct Pokemon *GetIllusionMonPtr(u32 battlerId)
{
    if (gBattleStruct->illusion[battlerId].broken)
        return NULL;
    if (!gBattleStruct->illusion[battlerId].set)
    {
        if (GetBattlerSide(battlerId) == B_SIDE_PLAYER)
            SetIllusionMon(&gPlayerParty[gBattlerPartyIndexes[battlerId]], battlerId);
        else
            SetIllusionMon(&gEnemyParty[gBattlerPartyIndexes[battlerId]], battlerId);
    }
    if (!gBattleStruct->illusion[battlerId].on)
        return NULL;

    return gBattleStruct->illusion[battlerId].mon;
}

void ClearIllusionMon(u32 battlerId)
{
    memset(&gBattleStruct->illusion[battlerId], 0, sizeof(gBattleStruct->illusion[battlerId]));
}

bool32 SetIllusionMon(struct Pokemon *mon, u32 battlerId)
{
    struct Pokemon *party, *partnerMon;
    s32 i, id;

    gBattleStruct->illusion[battlerId].set = 1;
    if (GetMonAbility(mon) != ABILITY_ILLUSION)
        return FALSE;

    if (GetBattlerSide(battlerId) == B_SIDE_PLAYER)
        party = gPlayerParty;
    else
        party = gEnemyParty;

    if (IsBattlerAlive(BATTLE_PARTNER(battlerId)))
        partnerMon = &party[gBattlerPartyIndexes[BATTLE_PARTNER(battlerId)]];
    else
        partnerMon = mon;

    // Find last alive non-egg pokemon.
    for (i = PARTY_SIZE - 1; i >= 0; i--)
    {
        id = i;
        if (GetMonData(&party[id], MON_DATA_SANITY_HAS_SPECIES)
            && GetMonData(&party[id], MON_DATA_HP)
            && &party[id] != mon
            && &party[id] != partnerMon)
        {
            gBattleStruct->illusion[battlerId].on = 1;
            gBattleStruct->illusion[battlerId].broken = 0;
            gBattleStruct->illusion[battlerId].partyId = id;
            gBattleStruct->illusion[battlerId].mon = &party[id];
            return TRUE;
        }
    }

    return FALSE;
}

bool8 ShouldGetStatBadgeBoost(u16 badgeFlag, u8 battlerId)
{
    return FALSE;
}

u8 GetBattleMoveSplit(u32 moveId)
{
    if (IS_MOVE_STATUS(moveId) || !FlagGet(FLAG_NO_SPLIT_MODE))
        return gBattleMoves[moveId].split;
    else if (gBattleMoves[moveId].type < TYPE_MYSTERY){
        if(!FlagGet(FLAG_INVERSE_MODE))
			return SPLIT_PHYSICAL;
		else
			return SPLIT_SPECIAL;
	}
    else{
        if(!FlagGet(FLAG_INVERSE_MODE))
			return SPLIT_SPECIAL;
		else
			return SPLIT_PHYSICAL;
	}
}

u8 GetBattleMoveSplitFromBattler(u32 moveId, u8 battlerId){
    u16 species = gBattleMons[battlerId].species;
    u8 formId = gBattleMons[battlerId].formId;
    u16 speciesID = GetFormSpeciesId(species, formId);
    u16 attackerAbility = GetBattlerAbility(battlerId);
    u16 atkStat = gBattleMons[battlerId].attack;
    u16 spAtkStat = gBattleMons[battlerId].spAttack;
    u8 hiddenPowerType;
    u8 highestStat;
    struct Pokemon *mon;

    if(atkStat > spAtkStat)
        highestStat = STAT_ATK;
    else
        highestStat = STAT_SPATK;

    // Gets mon data.
    if (GetBattlerSide(battlerId) == B_SIDE_OPPONENT)
        mon = &gEnemyParty[gBattlerPartyIndexes[battlerId]];
    else
        mon = &gPlayerParty[gBattlerPartyIndexes[battlerId]];

    hiddenPowerType = GetHiddenPowerTypeFromMon(mon);

    return GetBattleMoveSplitFromSpecies(moveId, speciesID, attackerAbility, hiddenPowerType, highestStat);
}

u8 GetBattleMoveSplitFromMon(u32 moveId, struct Pokemon *mon){
    u16 species = GetMonData(mon, MON_DATA_SPECIES);
    u8  formID = GetMonData(mon, MON_DATA_FORM_ID);
    u16 speciesID = GetFormSpeciesId(species, formID);
    u8  abilityNum = GetMonData(mon, MON_DATA_ABILITY_NUM);
    u16 atkStat = GetMonData(mon, MON_DATA_ATK);
    u16 spAtkStat = GetMonData(mon, MON_DATA_SPATK);
    u16 ability;
    u8 hiddenPowerType;
    u8 highestStat;

    hiddenPowerType = GetHiddenPowerTypeFromMon(mon);

    if(atkStat > spAtkStat)
        highestStat = STAT_ATK;
    else
        highestStat = STAT_SPATK;

    if(abilityNum != 2)
        ability = gBaseStats[speciesID].abilities[abilityNum];
    else
        ability = gBaseStats[speciesID].abilityHidden;

    return GetBattleMoveSplitFromSpecies(moveId, speciesID, ability, hiddenPowerType, highestStat);
}

u8 GetHiddenPowerFromBattler(u8 battler){
    u8 hpIvs = gBattleMons[battler].hpIV;
    u8 atkIvs = gBattleMons[battler].attackIV;
    u8 defIvs = gBattleMons[battler].defenseIV;
    u8 spAtkIvs = gBattleMons[battler].spAttackIV;
    u8 spDefIvs = gBattleMons[battler].spDefenseIV;
    u8 spdIvs = gBattleMons[battler].speedIV;
    return GetHiddenPowerType(hpIvs, atkIvs, defIvs, spAtkIvs, spDefIvs, spdIvs);
}

u8 GetHiddenPowerType(u8 hpIvs, u8 atkIvs, u8 defIvs, u8 spAtkIvs, u8 spDefIvs, u8 spdIvs){
    
    u8 moveType;
    u8 typeBits  = ((hpIvs & 1) << 0)
        | ((atkIvs & 1) << 1)
        | ((defIvs & 1) << 2)
        | ((spdIvs & 1) << 3)
        | ((spAtkIvs & 1) << 4)
        | ((spDefIvs << 5));

    u8 type = (15 * typeBits) / 63 + 1;
    if (type >= TYPE_MYSTERY)
        type++;
    type |= 0xC0;

    moveType = type & 0x3F;

    return moveType;
}

u8 GetHiddenPowerTypeFromMon(struct Pokemon *mon){
    u8 moveType;
    u8 typeBits  = ((GetMonData(mon, MON_DATA_HP_IV) & 1) << 0)
                 | ((GetMonData(mon, MON_DATA_ATK_IV) & 1) << 1)
                 | ((GetMonData(mon, MON_DATA_DEF_IV) & 1) << 2)
                 | ((GetMonData(mon, MON_DATA_SPEED_IV) & 1) << 3)
                 | ((GetMonData(mon, MON_DATA_SPATK_IV) & 1) << 4)
                 | ((GetMonData(mon, MON_DATA_SPDEF_IV) & 1) << 5);

    u8 type = (15 * typeBits) / 63 + 1;
        if (type >= TYPE_MYSTERY)
            type++;
    type |= 0xC0;

    moveType = type & 0x3F;

    return moveType;
}

u8 GetBattleMoveSplitFromSpecies(u32 moveId, u16 speciesID, u16 attackerAbility, u8 hiddenPowerType, u8 highestStat)
{
    u8 split = gBattleMoves[moveId].split;
    u8 MoveType = gBattleMoves[moveId].type;

    if(FlagGet(FLAG_NO_SPLIT_MODE) && gBattleMoves[moveId].effect != EFFECT_HIDDEN_POWER){
        if (gBattleMoves[moveId].type < TYPE_MYSTERY){
            if(!FlagGet(FLAG_INVERSE_MODE))
                split = SPLIT_PHYSICAL;
            else
                split = SPLIT_SPECIAL;
        }
        else{
            if(!FlagGet(FLAG_INVERSE_MODE))
                split = SPLIT_SPECIAL;
            else
                split = SPLIT_PHYSICAL;
        }
    }
    else if(gBattleMoves[moveId].effect == EFFECT_HIDDEN_POWER && FlagGet(FLAG_NO_SPLIT_MODE)){
        switch(hiddenPowerType){
            case TYPE_NORMAL:
            case TYPE_FIGHTING:
            case TYPE_FLYING:
            case TYPE_GROUND:
            case TYPE_ROCK:
            case TYPE_BUG:
            case TYPE_GHOST:
            case TYPE_POISON:
            case TYPE_STEEL:
                if(!FlagGet(FLAG_INVERSE_MODE))
                    split = SPLIT_PHYSICAL;
                else
                    split = SPLIT_SPECIAL;
            break;
            default:
				if(!FlagGet(FLAG_INVERSE_MODE))
                    split = SPLIT_SPECIAL;
                else
                    split = SPLIT_PHYSICAL;
            break;
        }
    }

    switch(attackerAbility){
        case ABILITY_PIXILATE://Fairy Type is an Special Move
            if(MoveType == TYPE_NORMAL
             && gBattleMoves[moveId].effect != EFFECT_HIDDEN_POWER
             && gBattleMoves[moveId].effect != EFFECT_WEATHER_BALL
             && gBattleMoves[moveId].effect != EFFECT_CHANGE_TYPE_ON_ITEM
             && gBattleMoves[moveId].effect != EFFECT_NATURAL_GIFT
             && FlagGet(FLAG_NO_SPLIT_MODE)){
                if(!FlagGet(FLAG_INVERSE_MODE))
                    split = SPLIT_SPECIAL;
                else
                    split = SPLIT_PHYSICAL;
             }
        break;
        case ABILITY_REFRIGERATE:
            if(MoveType == TYPE_NORMAL
             && gBattleMoves[moveId].effect != EFFECT_HIDDEN_POWER
             && gBattleMoves[moveId].effect != EFFECT_WEATHER_BALL
             && gBattleMoves[moveId].effect != EFFECT_CHANGE_TYPE_ON_ITEM
             && gBattleMoves[moveId].effect != EFFECT_NATURAL_GIFT
             && FlagGet(FLAG_NO_SPLIT_MODE)){
                if(!FlagGet(FLAG_INVERSE_MODE))
                    split = SPLIT_SPECIAL;
                else
                    split = SPLIT_PHYSICAL;
             }
        break;
        case ABILITY_AERILATE:
            if(MoveType == TYPE_NORMAL
             && gBattleMoves[moveId].effect != EFFECT_HIDDEN_POWER
             && gBattleMoves[moveId].effect != EFFECT_WEATHER_BALL
             && gBattleMoves[moveId].effect != EFFECT_CHANGE_TYPE_ON_ITEM
             && gBattleMoves[moveId].effect != EFFECT_NATURAL_GIFT
             && FlagGet(FLAG_NO_SPLIT_MODE)){
                if(!FlagGet(FLAG_INVERSE_MODE))
                    split = SPLIT_SPECIAL;
                else
                    split = SPLIT_PHYSICAL;
             }
        break;
        case ABILITY_GALVANIZE:
            if(MoveType == TYPE_NORMAL
             && gBattleMoves[moveId].effect != EFFECT_HIDDEN_POWER
             && gBattleMoves[moveId].effect != EFFECT_WEATHER_BALL
             && gBattleMoves[moveId].effect != EFFECT_CHANGE_TYPE_ON_ITEM
             && gBattleMoves[moveId].effect != EFFECT_NATURAL_GIFT
             && FlagGet(FLAG_NO_SPLIT_MODE)){
                if(!FlagGet(FLAG_INVERSE_MODE))
                    split = SPLIT_SPECIAL;
                else
                    split = SPLIT_PHYSICAL;
             }
        break;
        case ABILITY_LIQUID_VOICE:
            if(gBattleMoves[moveId].flags & FLAG_SOUND
             && FlagGet(FLAG_NO_SPLIT_MODE)){
                if(!FlagGet(FLAG_INVERSE_MODE))
                    split = SPLIT_SPECIAL;
                else
                    split = SPLIT_PHYSICAL;
             }
        break;
    }

    //For Signature Moves
    if(gSignatureMoveList[speciesID].move == moveId){
        if(gSignatureMoveList[speciesID].modification == SIGNATURE_MOD_PSS_CHANGE)
            split = gSignatureMoveList[speciesID].variable;
        else if (gSignatureMoveList[speciesID].modification2 == SIGNATURE_MOD_PSS_CHANGE)
            split = gSignatureMoveList[speciesID].variable2;
        else if (gSignatureMoveList[speciesID].modification3 == SIGNATURE_MOD_PSS_CHANGE)
            split = gSignatureMoveList[speciesID].variable3;
        else if (gSignatureMoveList[speciesID].modification4 == SIGNATURE_MOD_PSS_CHANGE)
            split = gSignatureMoveList[speciesID].variable4;
        else if (gSignatureMoveList[speciesID].modification5 == SIGNATURE_MOD_PSS_CHANGE)
            split = gSignatureMoveList[speciesID].variable5;
        else if (gSignatureMoveList[speciesID].modification6 == SIGNATURE_MOD_PSS_CHANGE)
            split = gSignatureMoveList[speciesID].variable6;
    }

    if(split == SPLIT_HIGHEST){
        if(highestStat == STAT_ATK)
            split = SPLIT_PHYSICAL;
        else
            split = SPLIT_SPECIAL;
    }

    if(split == SPLIT_WEAKEST){
        if(highestStat == STAT_ATK)
            split = SPLIT_SPECIAL;
        else
            split = SPLIT_PHYSICAL;
    }

    if (IS_MOVE_STATUS(moveId))
        split = gBattleMoves[moveId].split;

    return split;
}

void TryRestoreStolenItems(void)
{
    u32 i;
    u16 stolenItem = ITEM_NONE;

    if (B_RESTORE_ALL_ITEMS)
    {
        for (i = 0; i < PARTY_SIZE; i++)
        {
            if (gBattleStruct->itemStolen[i].originalItem != ITEM_NONE
                && GetMonData(&gPlayerParty[i], MON_DATA_HELD_ITEM, NULL) != gBattleStruct->itemStolen[i].originalItem)
            {
                stolenItem = gBattleStruct->itemStolen[i].originalItem;
                SetMonData(&gPlayerParty[i], MON_DATA_HELD_ITEM, &stolenItem);
            }
        }
    }
    else
    {
        for (i = 0; i < PARTY_SIZE; i++)
        {
            if (gBattleStruct->itemStolen[i].stolen)
            {
                stolenItem = gBattleStruct->itemStolen[i].originalItem;
                if (stolenItem != ITEM_NONE && ItemId_GetPocket(stolenItem) != POCKET_BERRIES)
                    SetMonData(&gPlayerParty[i], MON_DATA_HELD_ITEM, &stolenItem);  // Restore stolen non-berry items
            }
        }
    }
}

bool32 CanBePoisoned(u8 battlerAttacker, u8 battlerTarget){
    u16 ability = GetBattlerAbility(battlerTarget);
    
    if (!(CanPoisonType(battlerAttacker, battlerTarget))
     || gSideStatuses[GetBattlerSide(battlerTarget)] & SIDE_STATUS_SAFEGUARD
     || gBattleMons[battlerTarget].status1 & STATUS1_ANY
     || ability == ABILITY_IMMUNITY
     || ability == ABILITY_COMATOSE
     || IsAbilityOnSide(battlerTarget, ABILITY_PASTEL_VEIL)
     || gBattleMons[battlerTarget].status1 & STATUS1_ANY
     || IsAbilityStatusProtected(battlerTarget)
     || (gFieldStatuses & STATUS_FIELD_MISTY_TERRAIN)){
        return FALSE;
    }

    return TRUE;
}

bool32 CanSleep(u8 battlerId)
{
    u16 ability = GetBattlerAbility(battlerId);

    if (ability == ABILITY_INSOMNIA
      || ability == ABILITY_VITAL_SPIRIT
      || ability == ABILITY_COMATOSE
      || gSideStatuses[GetBattlerSide(battlerId)] & SIDE_STATUS_SAFEGUARD
      || gBattleMons[battlerId].status1 & STATUS1_ANY
      || IsAbilityOnSide(battlerId, ABILITY_SWEET_VEIL)
      || IsAbilityStatusProtected(battlerId)
      || (gFieldStatuses & STATUS_FIELD_ELECTRIC_TERRAIN)
      || (gFieldStatuses & STATUS_FIELD_MISTY_TERRAIN))
        return FALSE;
    return TRUE;
}

bool32 CanBeBurned(u8 battlerId)
{
    u16 ability = GetBattlerAbility(battlerId);
    if (IS_BATTLER_OF_TYPE(battlerId, TYPE_FIRE)
      || gSideStatuses[GetBattlerSide(battlerId)] & SIDE_STATUS_SAFEGUARD
      || gBattleMons[battlerId].status1 & STATUS1_ANY
      || ability == ABILITY_WATER_VEIL
      || ability == ABILITY_WATER_BUBBLE
      || ability == ABILITY_COMATOSE
      || IsAbilityStatusProtected(battlerId)
      || (gFieldStatuses & STATUS_FIELD_MISTY_TERRAIN))
        return FALSE;
    return TRUE;
}

bool32 CanBeParalyzed(u8 battlerAttacker, u8 battlerTarget)
{
    u16 ability = GetBattlerAbility(battlerTarget);
    if ((!CanParalyzeType(battlerAttacker, battlerTarget))
      || gSideStatuses[GetBattlerSide(battlerTarget)] & SIDE_STATUS_SAFEGUARD
      || ability == ABILITY_LIMBER
      || ability == ABILITY_COMATOSE
      || gBattleMons[battlerTarget].status1 & STATUS1_ANY
      || IsAbilityStatusProtected(battlerTarget)
      || (gFieldStatuses & STATUS_FIELD_MISTY_TERRAIN))
        return FALSE;
    return TRUE;
}

bool32 CanBeFrozen(u8 battlerId)
{
    u16 ability = GetBattlerAbility(battlerId);
    if (IS_BATTLER_OF_TYPE(battlerId, TYPE_ICE)
      || (gBattleWeather & WEATHER_SUN_ANY)
      || gSideStatuses[GetBattlerSide(battlerId)] & SIDE_STATUS_SAFEGUARD
      || ability == ABILITY_MAGMA_ARMOR
      || ability == ABILITY_COMATOSE
      || gBattleMons[battlerId].status1 & STATUS1_ANY
      || IsAbilityStatusProtected(battlerId)
      || (gFieldStatuses & STATUS_FIELD_MISTY_TERRAIN))
        return FALSE;
    return TRUE;
}

bool32 TestMoveFlags(u16 move, u32 flag)
{
    if (gBattleMoves[move].flags & flag)
        return TRUE;
    return FALSE;
}

// move checks
bool32 IsAffectedByPowder(u8 battler, u16 ability, u16 holdEffect)
{
    if (IS_BATTLER_OF_TYPE(battler, TYPE_GRASS)
      || ability == ABILITY_OVERCOAT
      || GetBattlerHoldEffect(battler, TRUE) == HOLD_EFFECT_SAFETY_GOGGLES)
        return FALSE;
    return TRUE;
}


bool32 DoesBattlerIgnoreAbilityChecks(u8 battler, u16 move)
{
    u32 i;
    u16 atkAbility = gBattleMons[battler].ability;
    
    /*for (i = 0; i < ARRAY_COUNT(sIgnoreMoldBreakerMoves); i++)
    {
        if (move == sIgnoreMoldBreakerMoves[i])
            return TRUE;
    }*/

    if (atkAbility == ABILITY_MOLD_BREAKER
      || atkAbility == ABILITY_TERAVOLT
      || atkAbility == ABILITY_TURBOBLAZE)
        return TRUE;

    return FALSE;
}

bool32 IsSemiInvulnerable(u8 battlerDef, u16 move)
{
    if (gStatuses3[battlerDef] & STATUS3_PHANTOM_FORCE)
        return TRUE;
    else if (!TestMoveFlags(move, FLAG_DMG_IN_AIR) && gStatuses3[battlerDef] & STATUS3_ON_AIR)
        return TRUE;
    else if (!TestMoveFlags(move, FLAG_DMG_UNDERWATER) && gStatuses3[battlerDef] & STATUS3_UNDERWATER)
        return TRUE;
    else if (!TestMoveFlags(move, FLAG_DMG_UNDERGROUND) && gStatuses3[battlerDef] & STATUS3_UNDERGROUND)
        return TRUE;
    else
        return FALSE;
}

bool32 IsMoveRedirectionPrevented(u16 move, u16 atkAbility)
{
    if (move == MOVE_SKY_DROP
      || move == MOVE_SNIPE_SHOT
      || atkAbility == ABILITY_PROPELLER_TAIL
      || atkAbility == ABILITY_STALWART)
        return TRUE;
    return FALSE;
}

bool32 IsAromaVeilProtectedMove(u16 move)
{
    u32 i;
    
    switch (move)
    {
    case MOVE_DISABLE:
    case MOVE_ATTRACT:
    case MOVE_ENCORE:
    case MOVE_TORMENT:
    case MOVE_TAUNT:
    case MOVE_HEAL_BLOCK:
        return TRUE;
    default:
        return FALSE;
    }
}

bool32 IsConfusionMoveEffect(u16 moveEffect)
{
    switch (moveEffect)
    {
    // case EFFECT_CONFUSE_HIT: This is only called for Electric Terrain, which shouldn't discourage Signal Beam et al.
    case EFFECT_SWAGGER:
    case EFFECT_FLATTER:
    case EFFECT_TEETER_DANCE:
        return TRUE;
    default:
        return FALSE;
    }
}

bool32 CanBeConfused(u8 battlerId)
{
    if (GetBattlerAbility(gEffectBattler) == ABILITY_OWN_TEMPO
      || gBattleMons[gEffectBattler].status2 & STATUS2_CONFUSION
      || (gFieldStatuses & STATUS_FIELD_MISTY_TERRAIN))
        return FALSE;
    return TRUE;
}

u16 GetUsedHeldItem(u8 battler)
{
    //return gBattleStruct->usedHeldItems[gBattlerPartyIndexes[battler]][GetBattlerSide(battler)];
    return gBattleStruct->usedHeldItems[battler] = ITEM_NONE;
}

u16 GetNaturePowerMove(void)
{
    if (gFieldStatuses & STATUS_FIELD_MISTY_TERRAIN)
        return MOVE_MOONBLAST;
    else if (gFieldStatuses & STATUS_FIELD_ELECTRIC_TERRAIN)
        return MOVE_THUNDERBOLT;
    else if (gFieldStatuses & STATUS_FIELD_GRASSY_TERRAIN)
        return MOVE_ENERGY_BALL;
    else if (gFieldStatuses & STATUS_FIELD_PSYCHIC_TERRAIN)
        return MOVE_PSYCHIC;
    else
        return MOVE_TRI_ATTACK;
}

u16 GetHigestAttackingStatFromBattler(u8 battlerId)
{
    u16 atkStat = gBattleMons[battlerId].attack;
    u16 spAtkStat = gBattleMons[battlerId].spAttack;

    if(atkStat > spAtkStat)
        return STAT_ATK;
    else
        return STAT_SPATK;
}

u8 isBattlerSingleTyped(u8 battler){
    u8 type1 = gBattleMons[battler].type1;
    u8 type2 = gBattleMons[battler].type2;
    u8 type3 = gBattleMons[battler].type3;

    if(type1 == type2 && type1 != TYPE_MYSTERY && type3 == TYPE_MYSTERY)
        return TRUE;
    else
        return FALSE;
}

u8 isSpeciesSingleTyped(u8 species){
    u8 type1 = gBaseStats[species].type1;
    u8 type2 = gBaseStats[species].type2;

    if(type1 == type2 && type1 != TYPE_MYSTERY)
        return TRUE;
    else
        return FALSE;
}

u8 getNumberOfPokemonWithTypesOnParty(u8 battler, u8 type){
    u8 i, j;
    u8 side = GetBattlerSide(battler);
    u8 battlerPartyIndex = gBattlerPartyIndexes[battler];
    u8 number = 0;
    u16 species, ability;
    u8 id, type1, type2;
    struct Pokemon *party;

    if(side == B_SIDE_PLAYER)
        party = gPlayerParty;
    else
        party = gEnemyParty;
    
    // Find last alive non-egg pokemon.
    for (i = 0; i <= PARTY_SIZE; i++)
    {
        id = i;
        species = GetMonData(&party[id], MON_DATA_SPECIES, NULL);
        type1 = gBaseStats[species].type1;
        type2 = gBaseStats[species].type2;
        ability = GetMonAbility(&party[id]);

        if (species != SPECIES_NONE             //Has to have an species
            && (type1 == type || type2 == type) //Checks type
            && id != battlerPartyIndex)         //It does not count the current battler
        {
            if(isSpeciesSingleTyped(species) || ability == ABILITY_ADAPTABILITY) //Single typed mons give a bigger boost
                number = number + 2;
            else
                number++;
        }
    }

    return number;
}
