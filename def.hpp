#include <vector>
#include <utility>

#include "model/Planet.hpp"
#include "model/FlyingWorkerGroup.hpp"
#include "model/Game.hpp"
#include "model/Specialty.hpp"

#include "danger_analysis.hpp"
#include "statistics.hpp"
#include "action_controller.hpp"
#include "precalc.hpp"
#include "simplex.hpp"
#include "utils/fraction.hpp"

struct group {
    int planet;
    int count;
};

/*
A Ограничение на число защитников:
    Для каждого j, (cумма по всем i d_i,j) <=
    (сумма по всем k при расстоянии от j до k меньше или равном расстоянию от i до j f_k,j) +
    (сумма по всем l на расстоянии строго меньше расстояния от i до j s_l).
B Ограничение на численность:
    Для каждого i: s_i + (сумма по всем j f_i,j) <= численность наших на i.
C Для каждой группы врагов переменную mx_i — сколько максимум врагов из нее прорвется. mx_i >=
    (численность группы - d_i.j) для всех i и j.


Минимизируем сумму mx_i
*/

/*
[planets] - vector of vectors of pair<group_id, dest_to_target>
[groups] - vector of EnemyGroup
[free_robots] - vector of pair<my_planet_id, count_of_robots>
[ac] - ActionController
[eps] - странная поебень
*/
template<class CalcType>
void setDefence(std::vector<std::vector<std::pair<int, int>>> &planets,
    const std::vector<EnemyGroup> &groups,
    const std::vector<int> &my_planets,
    const std::vector<std::pair<int, int>> &free_robots,
    ActionController &ac,
    CalcType eps = 0)
{
    using namespace std;
    using namespace model;

    /*ПЕРЕМЕННЫЕ*/

    //d_i_j

    int d_size = 0;
    vector<int> targets(groups.size(), 0);
    for(int i = 0; i < planets.size(); i++)
    {
        auto planet = planets[i];

        d_size += planet.size();

        for(auto g:planet)
        {
            targets[g.first]++;
        }
    }

    //f_i_j

    int f_size = free_robots.size() * planets.size();
    vector<group> myGroups;

    //s_i

    int s_size = free_robots.size();

    //mx_i

    int mx_size = groups.size();

    /*SOLVER*/

    int var_cnt = s_size + f_size + d_size + mx_size;
    int ind_d = 0;
    int ind_f = d_size;
    int ind_s = ind_f + s_size;
    int ind_mx = ind_s + mx_size;
    simplex_method::Solver<CalcType> solver(var_cnt);

    /*ОГРАНИЧЕНИЯ*/

    //B
    //for i s_i + sum(f_i_j, j) <= free_robots on i;
    //i - our planet
    //j - targeted planet

    for(int p = 0; p < free_robots.size(); p++)
    {
        vector<CalcType> coefs(var_cnt, 0);

        auto planet = free_robots[p];

        int robots = planet.second;

        coefs[ind_s + p] = 1;

        int index = p * planets.size();

        for(int i = 0; i < planets.size(); i++)
        {
            coefs[index + i] = 1;
        }

        solver.addLeConstraint(coefs, robots);
    }

    //C
    int index_tmp = 0;
    for(int i = 0; i < groups.size(); i++)
    {
        vector<CalcType> coefs(var_cnt, 0);

        for(int j = 0; j < targets[i]; j++)
        {
            coefs[index_tmp + j] = 1;
        }

        coefs[ind_mx + i] = 1;

        int count = groups[i].count;

        solver.addGeConstraint(coefs, count * targets[i]);

        index_tmp += targets[i];
    }

    //A

    for(int j = 0; j < planets.size(); j++)
    {
        vector<CalcType> coefs(var_cnt, 0);

        int tp_real_id = my_planets[j];

        auto p = planets[j];
        
        vector<pair<int, int>> g_Ids;

        sort(p.begin(), p.end(), [](pair<int, int> &a, pair<int, int> &b){
            return a.second > b.second;
        });

        for(int i = 0; i < p.size(); i++)
        {
            int groupId = p[i].first;

            int delta = 0;
            for(int g = 0; g < p[i].first; g++)
            {
                delta += targets[g];
            }

            int delta2 = 0;
            for(int l = 0; l < j; l++)
            {
                auto p2 = planets[l];
                for(auto g:p2)
                {
                    if(g.first == groupId)
                    {
                        delta2++;
                    }
                }
            }

            coefs[ind_d + delta + delta2] = -1;
            
            for(int s = 0; s < free_robots.size(); s++)
            {
                auto free_group = free_robots[s];
                int sp_real_id = my_planets[free_group.first];
                if(precalc::real_distance(tp_real_id, sp_real_id) < p[i].second)
                {
                    coefs[ind_s + s] = 1;
                }
            }
            for(int f = 0; f < free_robots.size(); f++)
            {
                auto free_group = free_robots[f];
                int sp_real_id = my_planets[free_group.first];
                if(precalc::real_distance(tp_real_id, sp_real_id) <= p[i].second)
                {
                    coefs[ind_f + f * planets.size() + j] = 1;
                }
            }

            solver.addGeConstraint(coefs, 0);
        }
    }

    /*РЕШЕНИЕ*/

    vector<CalcType> target_coefs(var_cnt, 0);
    for(int i = 0; i < groups.size(); i++)
    {
        target_coefs[ind_mx + i] = 1;
    }

    auto [t, ans] = solver.template solve<simplex_method::SolutionType::minimize>(move(target_coefs), eps);

    for(int i = 0; i < free_robots.size(); i++)
    {
        for(int j = 0; j < planets.size(); j++)
        {
            int start_planet_real_Id = my_planets[free_robots[i].first];
            int end_planet_real_Id = my_planets[j];
            ac.move(start_planet_real_Id, end_planet_real_Id, t[ind_f + i * planets.size() + j]).start();
        }
    }
}
