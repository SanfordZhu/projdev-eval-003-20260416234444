#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <sstream>
#include <iomanip>

using namespace std;

struct ProblemStatus {
    int wrong_before = 0;  // wrong submissions before first AC
    int solved = 0;        // 0 = not solved, 1 = solved
    int solve_time = 0;    // time of first AC
    int wrong_after_freeze = 0;  // submissions after freeze
    bool frozen = false;   // is this problem frozen?

    bool is_solved() const { return solved > 0; }
};

struct Team {
    string name;
    map<char, ProblemStatus> problems;
    int total_solved = 0;
    int total_penalty = 0;
    int ranking = 0;
};

struct Submission {
    string team_name;
    char problem;
    string status;
    int time;
};

class ICPCSystem {
private:
    bool started = false;
    bool frozen = false;
    int duration = 0;
    int problem_count = 0;
    map<string, Team> teams;
    vector<Submission> submissions;
    vector<string> ranking_order;  // team names in ranking order
    bool flushed = false;  // has scoreboard been flushed at least once?

public:
    void add_team(const string& name) {
        if (started) {
            cout << "[Error]Add failed: competition has started.\n";
            return;
        }
        if (teams.count(name)) {
            cout << "[Error]Add failed: duplicated team name.\n";
            return;
        }
        teams[name].name = name;
        cout << "[Info]Add successfully.\n";
    }

    void start_competition(int dur, int prob_cnt) {
        if (started) {
            cout << "[Error]Start failed: competition has started.\n";
            return;
        }
        started = true;
        duration = dur;
        problem_count = prob_cnt;
        // Initialize problems for all teams
        for (auto& [name, team] : teams) {
            for (int i = 0; i < problem_count; i++) {
                char prob = 'A' + i;
                team.problems[prob] = ProblemStatus();
            }
        }
        // Initial ranking by team name
        ranking_order.clear();
        for (const auto& [name, team] : teams) {
            ranking_order.push_back(name);
        }
        sort(ranking_order.begin(), ranking_order.end());
        update_rankings();
        cout << "[Info]Competition starts.\n";
    }

    void submit(const string& team_name, char problem, const string& status, int time) {
        if (!teams.count(team_name)) return;

        submissions.push_back({team_name, problem, status, time});
        Team& team = teams[team_name];
        ProblemStatus& prob = team.problems[problem];

        if (!prob.is_solved()) {
            if (status == "Accepted") {
                prob.solved = 1;
                prob.solve_time = time;
                team.total_solved++;
                team.total_penalty += 20 * prob.wrong_before + time;
            } else {
                if (frozen) {
                    prob.wrong_after_freeze++;
                } else {
                    prob.wrong_before++;
                }
            }
        }
    }

    void flush() {
        calculate_rankings();
        flushed = true;
        cout << "[Info]Flush scoreboard.\n";
    }

    void freeze() {
        if (frozen) {
            cout << "[Error]Freeze failed: scoreboard has been frozen.\n";
            return;
        }
        frozen = true;
        // Mark unsolved problems as frozen
        for (auto& [name, team] : teams) {
            for (auto& [prob, status] : team.problems) {
                if (!status.is_solved()) {
                    status.frozen = true;
                }
            }
        }
        cout << "[Info]Freeze scoreboard.\n";
    }

    void scroll() {
        if (!frozen) {
            cout << "[Error]Scroll failed: scoreboard has not been frozen.\n";
            return;
        }

        cout << "[Info]Scroll scoreboard.\n";
        flush();  // First flush

        // Output scoreboard before scrolling
        output_scoreboard();

        // Unfreeze one by one
        vector<pair<string, string>> ranking_changes;
        while (has_frozen_problems()) {
            // Find lowest ranked team with frozen problems
            string team_to_unfreeze;
            char prob_to_unfreeze;
            for (const auto& name : ranking_order) {
                for (int i = 0; i < problem_count; i++) {
                    char prob = 'A' + i;
                    if (teams[name].problems[prob].frozen) {
                        team_to_unfreeze = name;
                        prob_to_unfreeze = prob;
                        break;
                    }
                }
                if (!team_to_unfreeze.empty()) break;
            }

            if (team_to_unfreeze.empty()) break;

            // Unfreeze this problem
            teams[team_to_unfreeze].problems[prob_to_unfreeze].frozen = false;

            // Recalculate rankings
            auto old_ranking = ranking_order;
            calculate_rankings();

            // Check if ranking changed
            for (size_t i = 0; i < ranking_order.size(); i++) {
                if (i < old_ranking.size() && ranking_order[i] != old_ranking[i]) {
                    // Find which team moved up
                    string moved_up = ranking_order[i];
                    for (size_t j = i + 1; j < old_ranking.size(); j++) {
                        if (old_ranking[j] == moved_up) {
                            string replaced = old_ranking[i];
                            ranking_changes.push_back({moved_up, replaced});
                            break;
                        }
                    }
                    break;
                }
            }
        }

        // Output ranking changes
        for (const auto& [team1, team2] : ranking_changes) {
            const Team& t = teams[team1];
            cout << team1 << " " << team2 << " " << t.total_solved << " " << t.total_penalty << "\n";
        }

        // Output scoreboard after scrolling
        output_scoreboard();

        frozen = false;
    }

    void query_ranking(const string& team_name) {
        if (!teams.count(team_name)) {
            cout << "[Error]Query ranking failed: cannot find the team.\n";
            return;
        }
        cout << "[Info]Complete query ranking.\n";
        if (frozen) {
            cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
        }
        cout << "[" << team_name << "] NOW AT RANKING " << teams[team_name].ranking << "\n";
    }

    void query_submission(const string& team_name, const string& problem, const string& status) {
        if (!teams.count(team_name)) {
            cout << "[Error]Query submission failed: cannot find the team.\n";
            return;
        }
        cout << "[Info]Complete query submission.\n";

        // Find last submission matching criteria
        for (int i = submissions.size() - 1; i >= 0; i--) {
            const auto& sub = submissions[i];
            if (sub.team_name != team_name) continue;
            if (problem != "ALL" && string(1, sub.problem) != problem) continue;
            if (status != "ALL" && sub.status != status) continue;

            cout << "[" << team_name << "] " << sub.problem << " " << sub.status << " " << sub.time << "\n";
            return;
        }
        cout << "Cannot find any submission.\n";
    }

    void end() {
        cout << "[Info]Competition ends.\n";
    }

private:
    bool has_frozen_problems() const {
        for (const auto& [name, team] : teams) {
            for (const auto& [prob, status] : team.problems) {
                if (status.frozen) return true;
            }
        }
        return false;
    }

    void calculate_rankings() {
        vector<string> team_names;
        for (const auto& [name, team] : teams) {
            team_names.push_back(name);
        }

        sort(team_names.begin(), team_names.end(), [this](const string& a, const string& b) {
            const Team& ta = teams.at(a);
            const Team& tb = teams.at(b);

            // Count solved problems (excluding frozen ones)
            int solved_a = 0, solved_b = 0;
            int penalty_a = 0, penalty_b = 0;
            vector<int> solve_times_a, solve_times_b;

            for (const auto& [prob, status] : ta.problems) {
                if (!status.frozen && status.is_solved()) {
                    solved_a++;
                    penalty_a += 20 * status.wrong_before + status.solve_time;
                    solve_times_a.push_back(status.solve_time);
                }
            }
            for (const auto& [prob, status] : tb.problems) {
                if (!status.frozen && status.is_solved()) {
                    solved_b++;
                    penalty_b += 20 * status.wrong_before + status.solve_time;
                    solve_times_b.push_back(status.solve_time);
                }
            }

            if (solved_a != solved_b) return solved_a > solved_b;
            if (penalty_a != penalty_b) return penalty_a < penalty_b;

            // Compare solve times (descending order of solve times)
            sort(solve_times_a.rbegin(), solve_times_a.rend());
            sort(solve_times_b.rbegin(), solve_times_b.rend());

            for (size_t i = 0; i < max(solve_times_a.size(), solve_times_b.size()); i++) {
                int ta_time = (i < solve_times_a.size()) ? solve_times_a[i] : 0;
                int tb_time = (i < solve_times_b.size()) ? solve_times_b[i] : 0;
                if (ta_time != tb_time) return ta_time < tb_time;
            }

            return a < b;
        });

        ranking_order = team_names;
        update_rankings();
    }

    void update_rankings() {
        for (size_t i = 0; i < ranking_order.size(); i++) {
            teams[ranking_order[i]].ranking = i + 1;
        }
    }

    void output_scoreboard() {
        for (const auto& name : ranking_order) {
            const Team& team = teams[name];
            cout << name << " " << team.ranking << " " << team.total_solved << " " << team.total_penalty;

            // Calculate current solved and penalty (excluding frozen)
            int curr_solved = 0, curr_penalty = 0;
            for (const auto& [prob, status] : team.problems) {
                if (!status.frozen && status.is_solved()) {
                    curr_solved++;
                    curr_penalty += 20 * status.wrong_before + status.solve_time;
                }
            }

            // Output problem status
            for (int i = 0; i < problem_count; i++) {
                char prob = 'A' + i;
                auto it = team.problems.find(prob);
                const ProblemStatus& status = it->second;

                if (status.frozen) {
                    if (status.wrong_before == 0) {
                        cout << " 0/" << status.wrong_after_freeze;
                    } else {
                        cout << " -" << status.wrong_before << "/" << status.wrong_after_freeze;
                    }
                } else if (status.is_solved()) {
                    if (status.wrong_before == 0) {
                        cout << " +";
                    } else {
                        cout << " +" << status.wrong_before;
                    }
                } else {
                    if (status.wrong_before == 0) {
                        cout << " .";
                    } else {
                        cout << " -" << status.wrong_before;
                    }
                }
            }
            cout << "\n";
        }
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    ICPCSystem system;
    string line;

    while (getline(cin, line)) {
        if (line.empty()) continue;

        istringstream iss(line);
        string cmd;
        iss >> cmd;

        if (cmd == "ADDTEAM") {
            string name;
            iss >> name;
            system.add_team(name);
        } else if (cmd == "START") {
            string dummy1, dummy2, dummy3;
            int dur, prob_cnt;
            iss >> dummy1 >> dummy2 >> dur >> dummy3 >> prob_cnt;
            system.start_competition(dur, prob_cnt);
        } else if (cmd == "SUBMIT") {
            string problem, dummy1, team, dummy2, status, dummy3;
            int time;
            iss >> problem >> dummy1 >> team >> dummy2 >> status >> dummy3 >> time;
            system.submit(team, problem[0], status, time);
        } else if (cmd == "FLUSH") {
            system.flush();
        } else if (cmd == "FREEZE") {
            system.freeze();
        } else if (cmd == "SCROLL") {
            system.scroll();
        } else if (cmd == "QUERY_RANKING") {
            string name;
            iss >> name;
            system.query_ranking(name);
        } else if (cmd == "QUERY_SUBMISSION") {
            string team, dummy1, prob_part, dummy2, status_part;
            iss >> team >> dummy1 >> prob_part >> dummy2 >> status_part;

            // Parse PROBLEM=A
            string problem = prob_part.substr(8);
            // Parse STATUS=Accepted
            string status = status_part.substr(7);

            system.query_submission(team, problem, status);
        } else if (cmd == "END") {
            system.end();
            break;
        }
    }

    return 0;
}
