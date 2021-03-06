#ifndef PLAYER_STATS_H
#define PLAYER_STATS_H

enum stat_desc_type
{
    SD_NAME,
    SD_LOSS,
    SD_DECREASE,
    SD_INCREASE,
    NUM_STAT_DESCS
};

const char* stat_desc(stat_type stat);

bool attribute_increase();

void modify_stat(stat_type which_stat, int amount, bool suppress_msg);

void notify_stat_change(stat_type which_stat, int amount, bool suppress_msg);
void notify_stat_change();

string stat_name(stat_type stat);
string stat_abbreviation(stat_type stat);
stat_type nth_stat(int n);
vector<pair <int, stat_type>> ordered_stats();

void jiyva_stat_action();

int stat_loss_roll();
bool lose_stat(stat_type which_stat, int stat_loss, bool force = false);

stat_type random_lost_stat();
bool restore_stat(stat_type which_stat, int stat_gain,
                  bool suppress_msg, bool recovery = false);

int innate_stat(stat_type s);
#endif
