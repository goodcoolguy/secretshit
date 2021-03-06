#include "AppHdr.h"

#include <iostream>
#include <algorithm>
#include <vector>

#include "player-stats.h"

#include "artefact.h"
#include "clua.h"
#include "delay.h"
#include "files.h"
#include "godpassive.h"
#include "hints.h"
#include "item_use.h"
#include "libutil.h"
#include "macro.h"
#ifdef TOUCH_UI
#include "menu.h"
#endif
#include "message.h"
#include "misc.h"
#include "monster.h"
#include "mon-util.h"
#include "notes.h"
#include "ouch.h"
#include "output.h"
#include "player.h"
#include "religion.h"
#include "state.h"
#include "stringutil.h"
#ifdef TOUCH_UI
#include "tiledef-gui.h"
#include "tilepick.h"
#endif
#include "transform.h"

int player::stat(stat_type s, bool nonneg) const
{
    const int val = max(0, max_stat(s) - stat_loss[s]);
    return nonneg ? max(val, 0) : val;
}

//int player::strength(bool nonneg) const
//{
//    return stat(STAT_STR, nonneg);
//}

//int player::intel(bool nonneg) const
//{
//    return stat(STAT_INT, nonneg);
//}

//int player::dex(bool nonneg) const
//{
//    return stat(STAT_DEX, nonneg);
//}

static int _stat_modifier(stat_type stat, bool innate_only);

/**
 * What's the player's current maximum for a stat, before ability damage is
 * applied?
 *
 * @param s      The stat in question (e.g. STAT_STR).
 * @param innate Whether to disregard stat modifiers other than those from
 *               innate mutations.
 * @return      The player's maximum for the given stat; capped at
 *              MAX_STAT_VALUE.
 */
int player::max_stat(stat_type s, bool innate) const
{
    return min(base_stats[s] + _stat_modifier(s, innate), MAX_STAT_VALUE);
}

//int player::max_strength() const
//{
//    return max_stat(STAT_STR);
//}

//int player::max_intel() const
//{
//    return max_stat(STAT_INT);
//}

//int player::max_dex() const
//{
//    return max_stat(STAT_DEX);
//}

// Base stat including innate mutations (which base_stats does not)
int innate_stat(stat_type s)
{
    return you.max_stat(s, true);
}


static void _handle_stat_change(stat_type stat);

/**
 * Handle manual, permanent character stat increases. (Usually from every third
 * XL.
 *
 * @return Whether the stat was actually increased (HUPs can interrupt this).
 */
bool attribute_increase()
{
    const string stat_gain_message = make_stringf("Your experience leads to a%s "
                                                  "increase in your attributes!",
                                                  (you.species == SP_DEMIGOD || you.species == SP_TITAN) ?
                                                  " dramatic" : "n");
    crawl_state.stat_gain_prompt = true;
#ifdef TOUCH_UI
    learned_something_new(HINT_CHOOSE_STAT);
    Popup *pop = new Popup("Increase Attributes");
    MenuEntry *status = new MenuEntry("", MEL_SUBTITLE);
    pop->push_entry(new MenuEntry(stat_gain_message + " Increase:", MEL_TITLE));
    pop->push_entry(status);
    MenuEntry *me = new MenuEntry("Strength", MEL_ITEM, 0, 'S', false);
    me->add_tile(tile_def(TILEG_FIGHTING_ON, TEX_GUI));
    pop->push_entry(me);
    me = new MenuEntry("Intelligence", MEL_ITEM, 0, 'I', false);
    me->add_tile(tile_def(TILEG_SPELLCASTING_ON, TEX_GUI));
    pop->push_entry(me);
    me = new MenuEntry("Dexterity", MEL_ITEM, 0, 'D', false);
    me->add_tile(tile_def(TILEG_DODGING_ON, TEX_GUI));
    pop->push_entry(me);
#else
    mprf(MSGCH_INTRINSIC_GAIN, "%s", stat_gain_message.c_str());
    learned_something_new(HINT_CHOOSE_STAT);
    mprf(MSGCH_PROMPT, "Increase (M)elee, (D)efense, (R)anged, (S)tealth, " 
    "(B)lack Magic, (E)lemental Magic, or (T)haumaturgy? ");
#endif
    mouse_control mc(MOUSE_MODE_PROMPT);
	
    const int statgain = 1;

    bool tried_lua = false;
    int keyin;
    while (true)
    {
        // Calling a user-defined lua function here to let players reply to
        // the prompt automatically. Either returning a string or using
        // crawl.sendkeys will work.
        if (!tried_lua && clua.callfn("choose_stat_gain", 0, 1))
        {
            string result;
            clua.fnreturns(">s", &result);
            keyin = result[0];
        }
        else
        {
#ifdef TOUCH_UI
            keyin = pop->pop();
#else
            while ((keyin = getchm()) == CK_REDRAW)
                redraw_screen();
#endif
        }
        tried_lua = true;

        switch (keyin)
        {
        CASE_ESCAPE
            // It is unsafe to save the game here; continue with the turn
            // normally, when the player reloads, the game will re-prompt
            // for their level-up stat gain.
            if (crawl_state.seen_hups)
                return false;
            break;
			
        case 'm':
        case 'M':
            for (int i = 0; i < statgain; i++)
                modify_stat(STAT_MELEE, 1, false);
            return true;
        
        case 'r':
        case 'R':
            for (int i = 0; i < statgain; i++)
                modify_stat(STAT_RANGED, 1, false);
            return true;

        case 'd':
        case 'D':
            for (int i = 0; i < statgain; i++)
                modify_stat(STAT_DEFENSE, 1, false);
            return true;

	    case 's':
        case 'S':
            for (int i = 0; i < statgain; i++)
                modify_stat(STAT_SNEAK, 1, false);
            return true;

        case 'b':
        case 'B':
            for (int i = 0; i < statgain; i++)
                modify_stat(STAT_BLACK_MAGIC, 1, false);
            return true;

        case 'e':
        case 'E':
            for (int i = 0; i < statgain; i++)
                modify_stat(STAT_ELEMENTAL, 1, false);
            return true;
			
        case 't':
        case 'T':
            for (int i = 0; i < statgain; i++)
                modify_stat(STAT_THAUMATURGY, 1, false);
            return true;
	    
#ifdef TOUCH_UI
	    
        default:
            status->text = "Please choose an option below"; // too naggy?
#endif
        }
    }
}

/*
 * Have Jiyva increase a player stat by one and decrease a different stat by
 * one.
 *
 * This considers armour evp and skills to determine which stats to change. A
 * target stat vector is created based on these factors, which is then fuzzed,
 * and then a shuffle of the player's stat points that doesn't increase the l^2
 * distance to the target vector is chosen.
*/
void jiyva_stat_action()
{
    int cur_stat[NUM_STATS];
    int stat_total = 0;
    int target_stat[NUM_STATS];
    for (int x = 0; x < NUM_STATS; ++x)
    {
        cur_stat[x] = you.stat(static_cast<stat_type>(x), false);
        stat_total += cur_stat[x];
    }

    int evp = you.unadjusted_body_armour_penalty();
    //target_stat[STAT_STR] = max(9, evp);
    //target_stat[STAT_INT] = 9;
    //target_stat[STAT_DEX] = 9;
    int remaining = stat_total - 18;

    // Divide up the remaining stat points between Int and either Str or Dex,
    // based on skills.
    if (remaining > 0)
    {
        int magic_weights = 0;
        int other_weights = 0;
        for (skill_type sk = SK_FIRST_SKILL; sk < NUM_SKILLS; ++sk)
        {
            int weight = you.skills[sk];

            if (sk >= SK_FIRST_MAGIC_SCHOOL && sk <= SK_LAST_MAGIC)
                magic_weights += weight;
            else
                other_weights += weight;
        }
        // We give pure Int weighting if the player is sufficiently
        // focused on magic skills.
        other_weights = max(other_weights - magic_weights / 2, 0);

        // Now scale appropriately and apply the Int weighting
        magic_weights = div_rand_round(remaining * magic_weights,
                                       magic_weights + other_weights);
        other_weights = remaining - magic_weights;
        //target_stat[STAT_INT] += magic_weights;

        // Heavy armour weights towards Str, Dodging skill towards Dex.
        int str_weight = 10 * evp;
        int dex_weight = 10 + you.skill(SK_DODGING, 10);

        //target_stat[STAT_STR] += str_adj;
        //target_stat[STAT_DEX] += (other_weights - str_adj);
    }
    // Add a little fuzz to the target.
    for (int x = 0; x < NUM_STATS; ++x)
        target_stat[x] += random2(5) - 2;
    int choices = 0;
    int stat_up_choice = 0;
    int stat_down_choice = 0;
    // Choose a random stat shuffle that doesn't increase the l^2 distance to
    // the (fuzzed) target.
    for (int gain = 0; gain < NUM_STATS; ++gain)
        for (int lose = 0; lose < NUM_STATS; ++lose)
        {
            if (gain != lose && cur_stat[lose] > 1
                && target_stat[gain] - cur_stat[gain] > target_stat[lose] - cur_stat[lose]
                && cur_stat[gain] < MAX_STAT_VALUE && you.base_stats[lose] > 1)
            {
                choices++;
                if (one_chance_in(choices))
                {
                    stat_up_choice = gain;
                    stat_down_choice = lose;
                }
            }
        }
    if (choices)
    {
        simple_god_message("'s power touches on your attributes.");
        modify_stat(static_cast<stat_type>(stat_up_choice), 1, false);
        modify_stat(static_cast<stat_type>(stat_down_choice), -1, false);
    }
}

static const char* descs[NUM_STATS] = { "melee", "ranged", "defense", "stealth", "black magic", "elemental magic", "thaumaturgy" };

const char* stat_desc(stat_type stat)
{
    return descs[stat];
}

void modify_stat(stat_type which_stat, int amount, bool suppress_msg)
{
    ASSERT(!crawl_state.game_is_arena());

    // sanity - is non-zero amount?
    if (amount == 0)
        return;

    // Stop delays if a stat drops.
    if (amount < 0)
        interrupt_activity(AI_STAT_CHANGE);

    if (which_stat == STAT_RANDOM)
        which_stat = static_cast<stat_type>(random2(NUM_STATS));

    if (!suppress_msg)
    {
        mprf((amount > 0) ? MSGCH_INTRINSIC_GAIN : MSGCH_WARN,
             "Your %s %s.",
             stat_desc(which_stat), amount > 0 ? "improves" : "worsens");
    }

    you.base_stats[which_stat] += amount;

    _handle_stat_change(which_stat);
}

void notify_stat_change(stat_type which_stat, int amount, bool suppress_msg)
{
    ASSERT(!crawl_state.game_is_arena());

    // sanity - is non-zero amount?
    if (amount == 0)
        return;

    // Stop delays if a stat drops.
    if (amount < 0)
        interrupt_activity(AI_STAT_CHANGE);

    if (which_stat == STAT_RANDOM)
        which_stat = static_cast<stat_type>(random2(NUM_STATS));

    if (!suppress_msg)
    {
        mprf((amount > 0) ? MSGCH_INTRINSIC_GAIN : MSGCH_WARN,
             "Your %s %s.",
             stat_desc(which_stat), amount > 0 ? "improves" : "worsens");
    }

    _handle_stat_change(which_stat);
}

void notify_stat_change()
{
    for (int i = 0; i < NUM_STATS; ++i)
        _handle_stat_change(static_cast<stat_type>(i));
}

static int _mut_level(mutation_type mut, bool innate_only)
{
    return you.get_base_mutation_level(mut, true, !innate_only, !innate_only);
}

static int _stat_modifier(stat_type stat, bool innate_only)
{
  return 0;
    switch (stat)
    {
    default:
        mprf(MSGCH_ERROR, "Bad stat: %d", stat);
        return 0;
    }
}

string stat_name(stat_type stat)
{
    switch (stat)
    {
    case STAT_MELEE:
        return "melee";
    case STAT_RANGED:
        return "ranged";
    case STAT_SNEAK:
        return "stealth";
    case STAT_DEFENSE:
        return "defense";
    case STAT_BLACK_MAGIC:
        return "black magic";
    case STAT_ELEMENTAL:
        return "elemental magic";
    case STAT_THAUMATURGY:
        return "thaumaturgy";
    default:
        die("invalid stat");
    }
}

string stat_abbreviation(stat_type stat)
{
    switch (stat)
    {
    case STAT_MELEE:
        return "mel";
    case STAT_RANGED:
        return "rng";
    case STAT_SNEAK:
        return "sth";
    case STAT_DEFENSE:
        return "def";
    case STAT_BLACK_MAGIC:
        return "blk";
    case STAT_ELEMENTAL:
        return "ele";
    case STAT_THAUMATURGY:
        return "thm";
    default:
        die("invalid stat");
    }
}

//return the player's nth highest stat
stat_type nth_stat(int n)
{
    vector<pair <int, stat_type>> stats = ordered_stats();
    return stats[n-1].second;
}

//provide a sorted vector of stats in descending order
vector<pair <int, stat_type>> ordered_stats()
{
	vector<pair <int, stat_type>> ret;
	for(int s = (int) STAT_MELEE; s < (int) NUM_STATS; s++)
    {
        stat_type stat = static_cast<stat_type>(s);
        ret.emplace_back(make_pair(you.stat(stat), stat));
    }
    sort(ret.begin(),ret.end());
    reverse(ret.begin(),ret.end());
    return ret;
}

int stat_loss_roll()
{
    const int loss = 30 + random2(30);
    dprf("Stat loss points: %d", loss);

    return loss;
}

bool lose_stat(stat_type which_stat, int stat_loss, bool force)
{
    if (stat_loss <= 0)
        return false;

    if (which_stat == STAT_RANDOM)
        which_stat = static_cast<stat_type>(random2(NUM_STATS));

    mprf(MSGCH_WARN, "Your %s worsens.", stat_desc(which_stat));

    you.stat_loss[which_stat] = min<int>(100,
                                         you.stat_loss[which_stat] + stat_loss);
    if (!you.attribute[ATTR_STAT_LOSS_XP])
        you.attribute[ATTR_STAT_LOSS_XP] = stat_loss_roll();
    _handle_stat_change(which_stat);
    return true;
}

stat_type random_lost_stat()
{
    stat_type choice = NUM_STATS;
    int found = 0;
    for (int i = 0; i < NUM_STATS; ++i)
        if (you.stat_loss[i] > 0)
        {
            found++;
            if (one_chance_in(found))
                choice = static_cast<stat_type>(i);
        }
    return choice;
}

// Restore the stat in which_stat by the amount in stat_gain, displaying
// a message if suppress_msg is false, and doing so in the recovery
// channel if recovery is true. If stat_gain is 0, restore the stat
// completely.
bool restore_stat(stat_type which_stat, int stat_gain,
                  bool suppress_msg, bool recovery)
{
    // A bit hackish, but cut me some slack, man! --
    // Besides, a little recursion never hurt anyone {dlb}:
    if (which_stat == STAT_ALL)
    {
        bool stat_restored = false;
        for (int i = 0; i < NUM_STATS; ++i)
            if (restore_stat((stat_type) i, stat_gain, suppress_msg))
                stat_restored = true;

        return stat_restored;
    }

    if (which_stat == STAT_RANDOM)
        which_stat = random_lost_stat();

    if (which_stat >= NUM_STATS || you.stat_loss[which_stat] == 0)
        return false;

    if (stat_gain == 0 || stat_gain > you.stat_loss[which_stat])
        stat_gain = you.stat_loss[which_stat];

    you.stat_loss[which_stat] -= stat_gain;

    // If we're fully recovered, clear out stat loss recovery timer.
    if (random_lost_stat() == NUM_STATS)
        you.attribute[ATTR_STAT_LOSS_XP] = 0;

    _handle_stat_change(which_stat);
    return true;
}

static void _normalize_stat(stat_type stat)
{
    ASSERT(you.stat_loss[stat] >= 0);
    you.base_stats[stat] = min<int8_t>(you.base_stats[stat], MAX_STAT_VALUE);
}

static void _handle_stat_change(stat_type stat)
{
    ASSERT_RANGE(stat, 0, NUM_STATS);

    you.redraw_stats[stat] = true;
    _normalize_stat(stat);
}
