#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <SFML/System/Clock.hpp>
#include <SFML/System.hpp>

#include "specificity.h"
#include "utils/sparse_array.h"
#include "sun_lambda.h"

using byte = uint8_t;
#define SpecificityDepth 4
using ScheduleSpecificity = Specificity<SpecificityDepth>;

/*
 * Bicycle Mango
 * A hopeful gameplay framework
 * by Wesxdz and lapinozz
 * fast and free for (0b11111111 : while(true))
 *
 *
 *      ***               o000P
 *          \               |.
 *           |_____________/_----____/
 *           // \             \\
 *          //   \             \\       ___
 *     00000/     \             000011 /  v \
 *   00\-|--00     \          00\-|--11  v  /
 *  (0---O---0)     ( λ )----(0--xOx--0)   V
 *   00/-|--00        |-------00/-|--00
 *     00000         ^^^        00000
 *
 *
 * The role of a game engine is to empower the negotiation of imagination.
 * Creating a game should be fun.
 *
 */
class BicycleMango
{
public:
    // The time between the beginning and end of the last loop
    static inline sf::Time delta;

    static inline sf::Time targetFrameRate = sf::seconds(1.0f/144.0f);
    
    // Should the Bicycle Mango gameplay loop stop?
    static inline bool brake = false;
    
    struct SunSchedule
    {
        SunLambda::Id id;
        ScheduleSpecificity specificity;
    };
    static inline std::vector<SunSchedule> schedules;

    static void Loop()
    {
        sf::Clock clock;
        auto startTime = clock.getElapsedTime();
        CreatePropsDelayed();
        for (const SunSchedule& schedule : schedules)
        {
            SunLambdaRegistry::GetInstance().Get(schedule.id)();
        }
        RemovePropsDelayed();

#ifdef HOT_RELOAD
        if(ShouldReloadLambdas)
        {
            ReloadLambdas();
            ShouldReloadLambdas = false;
        }
#endif

        delta = clock.getElapsedTime() - startTime;
        if (delta < targetFrameRate)
        {
            sf::sleep(targetFrameRate - delta);
            delta = targetFrameRate;
        }
    }

#ifdef HOT_RELOAD
    static inline bool ShouldReloadLambdas = false;
    static void ReloadLambdas()
    {
        SunLambdaRegistry::GetInstance().Unload();

        system(HOT_RELOAD_CMAKE " --build " HOT_RELOAD_BUILD_PATH " --target " HOT_RELOAD_TARGET);

        SunLambdaRegistry::GetInstance().Reload();
    }
#endif

    // An internal id used to access vectors of props of a specific struct type
    using PropTypeId = size_t;
    
    // Props are stored in contiguous vectors and are accessed sequentially by their raw id
    using PropIdRaw = size_t;
    
    // ALL STAGES OF A PROP MUST RETURN TRUE ON THE COMPATIBILITY CONSTRAINT FOR THAT PROP TO BE CONSIDERED TO FORM PART OF A NOVEL TUPLE OF THE SunLambda::Id WHICH HAS THIS CompatibleConstraint AS PART OF ITS NovelTupleCreator
    // A user provided function set per SunLambda that determines if props can be considered part of a novel tuple
    using CompatibleConstraint = std::function<bool(PropTypeId, const Stage&)>;

    // A user provided that represents if this prop should be reused in novel tuples
    // Must return true for all other props in the typeset
    using PartialStaticIndicators = std::unordered_map<PropTypeId, std::function<bool(GroupSet&, PropTypeId, GroupSet&)>>;

    struct NovelTupleCreator
    {
        CompatibleConstraint compatible;
        PartialStaticIndicators reuseOnStages;
    };

    // Emerges are called when a novel tuple is formed
    static inline std::set<SunLambda::Id> emerges;
    static void Emerge(SunLambda::Id id, CompatibleConstraint compatible = All)
    {
        emerges.insert(id);
        novelTupleCreators[id].compatible = compatible;
    }

    // Breakups are called when a one or more of the props in a novel tuple are removed
    static inline std::set<SunLambda::Id> breakups;
    static void Breakup(SunLambda::Id id, CompatibleConstraint compatible = All)
    {
        breakups.insert(id);
        novelTupleCreators[id].compatible = compatible;
    }

    /*
     * Find a prop's raw id from its pointer address and type
     * This is required for storing references to props for access in subsequent SunLambdas
    */
    template <typename T>
    static PropIdRaw GetPropId(void* propAddress)
    {
        auto& props = GetProps<T>();
        uintptr_t offset = (reinterpret_cast<uintptr_t>(propAddress) - reinterpret_cast<uintptr_t>(&props.buffer[0]));
        PropIdRaw pidr = offset/(sizeof(T) + sizeof(bool));
        return pidr;
    }

    // Shared between emerge/plan/breakup: we assume a SunLambda cannot have multiple NovelTupleCreators for now
    static inline std::unordered_map<SunLambda::Id, NovelTupleCreator> novelTupleCreators;

    // This data structure is an augment of potential neighbors when searching for props to form novel tuples
    static inline std::unordered_map<SunLambda::Id, std::unordered_map<PropTypeId, std::vector<PropIdRaw>>> partialStatics;

    static inline CompatibleConstraint All = [](PropTypeId, const Stage&){return true;};
    static void Plan(SunLambda::Id id, const ScheduleSpecificity& specificity, CompatibleConstraint compatible = All)
    {
        SunSchedule schedule{id, specificity};

        auto it = std::upper_bound(schedules.begin(), schedules.end(), schedule,
        [](const SunSchedule& a, const SunSchedule& b) -> bool {
            return a.specificity < b.specificity;
        });

        schedules.insert(it, schedule);
        novelTupleCreators[id].compatible = compatible;
    }

    template<typename T>
    struct PropId { PropIdRaw id; };

    // I decided to place PropTypeId before Group this data structure because it is more commonly used this way in the code, even though the console works by query by group
    // TODO Will we need this later for faster console querying??
//     using GroupTypeInstance = std::unordered_map<PropTypeId, std::unordered_map<Group, std::set<std::pair<Instance, PropIdRaw>>>>;

    static inline std::unordered_map<PropTypeId, std::unordered_map<PropIdRaw, std::set<Stage>>> ptpsq; // Prop type prop stages query
    // SHOULD WE ONLY STORE THIS INFORMATION FOR PARTIAL STATICS OR EVERYTHING???
//     static inline std::unordered_map<PropTypeId, std::unordered_map<Group, std::unordered_map<Instance, std::set<PropIdRaw>>>> ptgid;
    
    struct GlobalPropId {PropTypeId typeId; PropIdRaw id;};
    friend inline bool operator< (const GlobalPropId& lhs, const GlobalPropId& rhs){ return lhs.typeId < rhs.typeId && lhs.id < rhs.id; }


    // Rather ironically, Typeset is a vector because the prop types must be delivered in a certain order to the SunLambda functor despite the conceptual set of props being acted upon
    using Typeset = std::vector<PropTypeId>;
    // All SunLambda and Jolt typeset signatures used (we should only keep track of novel tuples of typesets that are used by gameplay programmers by adding considered typesets in the DefineSunLambda constructor)
    
    static inline std::unordered_map<PropTypeId, std::set<Typeset>> mappedPropTupleTypesets;
    static inline std::set<Typeset> globalPropTupleTypesets;

    // Each SunLambda stores a map of props of types in its typeset which have been added but not formed into novel tuples yet
    // Partial static indicators that return true indicate that a prop can be reused in multiple novel tuples of the same SunLambda
    static inline std::map<SunLambda::Id, std::unordered_map<PropTypeId, std::vector<PropIdRaw>>> stagingPropTuples;
    
    static inline std::map<SunLambda::Id, std::vector<std::vector<GlobalPropId>>> novelTuples;
    static inline std::unordered_map<SunLambda::Id, Typeset> sunLambdaTypesets;
    static inline std::map<Typeset, std::vector<SunLambda::Id>> typesetSunLambdas;

    template<typename... PTypes>
    static void ConsiderTypeset(SunLambda::Id id)
    {
        Typeset typeset = {BicycleMango::GetPropTypeId<std::decay_t<PTypes>>()...};
        sunLambdaTypesets[id] = typeset;
        typesetSunLambdas[typeset].push_back(id);
        for (PropTypeId ptid : typeset)
        {
            BicycleMango::globalPropTupleTypesets.insert(typeset);
            BicycleMango::mappedPropTupleTypesets[ptid].insert(typeset);
        }
    }
    
    template <typename... PTypes>
    static void CallJolt(void (*functor)(PTypes...), SunLambda::Id id, std::vector<PropIdRaw>& sunData)
    {
        CallJolt<PTypes...>(functor, id, sunData, std::index_sequence_for<PTypes...> {});
        
    }
    
    template <typename... PTypes, std::size_t ... Is>
    static void CallJolt(void (*functor)(PTypes...), SunLambda::Id id, std::vector<PropIdRaw>& sunData, std::index_sequence<Is...> seq)
    {
//         std::cout << "Calling jolt with " << sizeof...(PTypes) << " props and " << sunData.size() << " sun data!" << std::endl;
        functor(GetProps<std::decay_t<PTypes>>()[sunData[Is]]...);
    }

    static inline std::unordered_map<PropTypeId, std::function<void(PropIdRaw)>> freeFunctions;

    // Prop ----------------
    static inline std::unordered_map<PropTypeId, std::string> propTypeNames;

    template <typename T>
    static constexpr PropTypeId GetPropTypeId()
    {
        const PropTypeId id = std::type_index(typeid(std::decay_t<T>)).hash_code();
        propTypeNames[id] = std::type_index(typeid(std::decay_t<T>)).name(); 
        freeFunctions[id] = [&](PropIdRaw pidr){GetProps<T>().free(pidr);};
        return id;
    }
    
    static inline std::unordered_map<Group, IdPool<Instance>> instanceBuffer;
    
    static Stage Next(Group group)
    {
        return {group, instanceBuffer[group].next()};
    }

    template<typename PropType>
    static void AddPropStage(PropId<PropType> propId, const Stage& stage)
    {
        instanceBuffer[stage.group].nextId = stage.instance + 1; // This is kind of a hack, and could cause some large sections of unused ids potentially but I think it will work
        ptpsq[GetPropTypeId<PropType>()][propId.id].insert(stage);
//         ptgid[GetPropTypeId<PropType>()][stage.group][stage.instance].insert(propId.id);
    }

    template<typename PropType>
    static void AddPropStages(PropId<PropType> propId, const GroupSet& stages)
    {
        for(const Stage& stage : stages)
        {
            AddPropStage(propId, stage);
        }
    }

    struct DelayedPropCreator
    {
        GlobalPropId gpid;
        GroupSet stages;
    };

    static void AddPropStages(DelayedPropCreator& creator)
    {
        for(const Stage& stage : creator.stages)
        {
            instanceBuffer[stage.group].nextId = stage.instance + 1;
            ptpsq[creator.gpid.typeId][creator.gpid.id].insert(stage);
        }
    }

    template<typename PropType>
    static PropType* InitProp(const GroupSet& stages)
    {
        auto [id, prop] = GetProps<PropType>().next();
        PropId<PropType> propId{id};
        AddPropStages(propId, stages);
        ConsiderProp({GetPropTypeId<PropType>(), id});
        return &prop;
    }

    // Added props are allocated immediately for factory construction but only form novel tuples at the beginning of a frame (after removal queries are executed) 
    template<typename PropType>
    static PropType* AddProp(const GroupSet& stages)
    {
        auto [id, prop] = GetProps<PropType>().next();
        PropId<PropType> propId{id};
        propsToAdd.push_back({{GetPropTypeId<PropType>(), id}, stages});
        return &prop;
    }

    static void CreatePropsDelayed()
    {
        for (DelayedPropCreator& creator : propsToAdd)
        {
            AddPropStages(creator);
            ConsiderProp(creator.gpid);
        }
        propsToAdd.clear();
    }
    
    static inline std::vector<DelayedPropCreator> propsToAdd;

    static void ConsiderProp(GlobalPropId consider)
    {
        PropIdRaw id = consider.id;
        PropTypeId propTypeId = consider.typeId;

        GroupSet& stages = ptpsq[propTypeId][id];
        // std::cout << "--- Adding prop {" << propTypeNames[propTypeId] << ", " << id << "}" << std::endl;
        auto typesetsWithAddedPropType = BicycleMango::mappedPropTupleTypesets[propTypeId];
  
        // ---
        for (auto typeset_it = typesetsWithAddedPropType.begin(); typeset_it != typesetsWithAddedPropType.end(); ++typeset_it)
        {
            for (auto sunlambda_it = typesetSunLambdas[(*typeset_it)].begin();
                 sunlambda_it != typesetSunLambdas[(*typeset_it)].end(); 
                ++sunlambda_it)
            {
        // --- 'for each SunLambda that PropType is a parameter of'
                // std::cout << "Checking for novel tuple on SunLambda: " << SunLambdaRegistry::GetInstance().Get((*sunlambda_it)).name << std::endl;
                // Check if a novel tuple is formed with this prop from the staged prop neighbors of each SunLambda that propTypeId is in!
                // Partial statics are not considered as potentialNeighbors because then we'd have to copy the stagingPropTuples vector rather than using a ref
                std::unordered_map<PropTypeId, std::vector<PropIdRaw>>& potentialNeighbors = BicycleMango::stagingPropTuples[(*sunlambda_it)];
                // However, both potential neighbors AND partial statics will be added to compatibleNeighbors later in this function
                std::unordered_map<PropTypeId, std::vector<PropIdRaw>> compatibleNeighbors;
                bool addedPropFulfillsCompatabilityConstraint = false;
                bool novelTupleRuledOut = false;
                
                auto IsPropCompatibleWithSunLambda = [sunlambda_it](PropTypeId ptid, PropIdRaw rid) -> bool
                {
                    // std::cout << propTypeNames[ptid] << ", " << "id: " << rid << " on SunLambda: " << SunLambdaRegistry::GetInstance().Get((*sunlambda_it)).name << std::endl;
                    
                    for (const Stage& stage : ptpsq[ptid][rid])
                    {
                        // std::cout << "Checking compatability on stage: " << stage << " for {" << propTypeNames[ptid] << ", " << rid << "}" << std::endl;
                        if (!novelTupleCreators[(*sunlambda_it)].compatible)
                        {
                            // The prop was added before this SunLambda was planned!
                            // TODO: Add novel tuples on SunLambda registry in case tuples were added and staged prior to planning
                            // Figure out how a CompatibleConstraint should be set prior to planning
                            return true;
                        }
                        if (novelTupleCreators[(*sunlambda_it)].compatible(ptid, stage))
                        {
                            // std::cout << "COMPATIBLE!" << std::endl;
                            return true;
                        }
                    }
                    return false;
                };
                
                // The first thing we need to check is if the prop we're adding fulfills the compatability constraint for this SunLambda, if not then we can short circut everything
                if (IsPropCompatibleWithSunLambda(propTypeId, id))
                {
                    addedPropFulfillsCompatabilityConstraint = true;
                } else
                {
                    novelTupleRuledOut = true;
                    // std::cout << "Prop does not fulfill compatability constraint!" << std::endl;
                }
                
                NovelTupleCreator& creator = novelTupleCreators[(*sunlambda_it)];
                bool isAddedPropPartialStatic = false;
                if (!novelTupleRuledOut)
                {
                    if (novelTupleCreators.count((*sunlambda_it)) > 0)
                    {
                        if (creator.reuseOnStages.count(propTypeId) > 0) // All props with reuse function are considered partial statics
                        {
                            isAddedPropPartialStatic = true;
                            partialStatics[(*sunlambda_it)][propTypeId].push_back(id);
                        }
                    }
                }
                
                std::unordered_map<PropTypeId, PropIdRaw> partialStaticNeighbors;
                auto FindPartialStatic = [sunlambda_it, &partialStaticNeighbors, &IsPropCompatibleWithSunLambda, &creator, &stages, &propTypeId](PropTypeId ptid)
                {
                    for (auto& partialStaticsOfType : partialStatics[(*sunlambda_it)])
                    {
                        for (const PropIdRaw& partialStatic : partialStaticsOfType.second)
                        {
                            
                            if (IsPropCompatibleWithSunLambda(ptid, partialStatic))
                            {;
                                // TODO: Maybe use a different data structure for efficiency, it depends how many partial statics there could be
                                // creator.reuseOnStages[propTypeId](stages, partialStaticsOfType.first, ptpsq[partialStaticsOfType.first][partialStatic])
                                if (true)
                                {
                                    // std::cout << "Found partial static: " << propTypeNames[ptid] << std::endl;
                                    partialStaticNeighbors[ptid] = partialStatic;
                                    return true;
                                }
                            } else
                            {
                                // std::cout << "Partial static not compatible!" << std::endl;
                            }
                        }
                    }
                    return false;
                };
                
                if (!novelTupleRuledOut)
                {
                    // We need to make sure that potentialNeighbors has at least one prop of each type in this SunLambda::Id other than the type of prop being added, otherwise we can rule out creating a novel tuple
                    for (PropTypeId ptid : (*typeset_it))
                    {
                        if (ptid != propTypeId && potentialNeighbors[ptid].empty())
                        {
                            if (!FindPartialStatic(ptid))
                            {
                                novelTupleRuledOut = true;
                                break;
                            }
                        }
                    }
                }
                
                if (!novelTupleRuledOut)
                {
                    for (auto neighbor_it = potentialNeighbors.begin(); neighbor_it != potentialNeighbors.end(); ++neighbor_it)
                    {
                        if ((*neighbor_it).first == propTypeId || partialStaticNeighbors.count((*neighbor_it).first)) continue;
                        if (partialStaticNeighbors.count((*neighbor_it).first)) continue; // If there is a partial static neighbor, let's just use that :)
                        for (auto& neighborId : (*neighbor_it).second)
                        {
//                                 std::cout << "Let's go " << propTypeNames[(*neighbor_it).first] << " has " << (*neighbor_it).second.size() << std::endl;
                            if (IsPropCompatibleWithSunLambda((*neighbor_it).first, neighborId))
                            {
                                // std::cout << "Found it!" << std::endl;
                                compatibleNeighbors[(*neighbor_it).first].push_back(neighborId);
                                break; // Not sure what to do if there are multiple compatible neighbors, for now we just FIFO after considering compatability
                            }
                        }
                        if (compatibleNeighbors[(*neighbor_it).first].empty()) 
                        {
                            // std::cout << "Compatible neighbors is empty on " << propTypeNames[(*neighbor_it).first] << " while the prop type being added is " << propTypeNames[propTypeId] << std::endl;
                            novelTupleRuledOut = true;
                            break; // No compatible neighbors of ptid to pair with
                        }
                    }
                }
                // Compatible neighbors is now populated with props that are compatible with (*sunlambda_it)
                std::vector<GlobalPropId> novelTuple((*typeset_it).size());
                
                int addedPropTypeIndex = 0;
                if (!novelTupleRuledOut)
                {
                    int i = 0;
                    for (const PropTypeId& ptid : (*typeset_it))
                    {
                        if (ptid == propTypeId) 
                        {
                            addedPropTypeIndex = i;
                            i++;
                            continue;
                        }
                        if (partialStaticNeighbors.count(ptid))
                        {
                            novelTuple[i] = {ptid, partialStaticNeighbors[ptid]};
                        } else
                        {
                            novelTuple[i] = {ptid, compatibleNeighbors[ptid].back()};
                            // TODO: This is incorrect, potential neighbors should remove the greatest compatible index found in potentialNeighbors
                            potentialNeighbors[ptid].pop_back();
                        }
                        i++;
                    }
                    
                    // std::cout << "Formed novel tuple of " << SunLambdaRegistry::GetInstance().Get((*sunlambda_it)).name << " with the following props:" << std::endl;
                    
                    
                    // The prop that we're adding should be included in the novel tuple! Hooray!
                    novelTuple[addedPropTypeIndex] = {propTypeId, id};
                    // The prop that we're adding should be removed from potential neighbors
    //                 std::cout << "Inserting novel tuple of " << novelTuple.size() << " size" << std::endl;
                    novelTuples[(*sunlambda_it)].push_back(novelTuple);
                    
                    // This could be optimized with an emerge only sunLambdaTypesets data structure
                    for (auto emergesun_it = emerges.begin(); emergesun_it != emerges.end(); ++emergesun_it)
                    {
                        if (sunLambdaTypesets[(*emergesun_it)] == (*typeset_it))
                        {
                            std::vector<PropIdRaw> sunData(novelTuple.size()); // The SunLambda already knows the types in order, therefore we only need pass it the PropIdRaw values and it can imply the types
                            for (GlobalPropId& gpid : novelTuple)
                                sunData.push_back(gpid.id);
                            SunLambda& sun = SunLambdaRegistry::GetInstance().Get(*(emergesun_it));
                            sun.Emerge(sunData);
                            break;
                        }
                    }
                    
                } 
                else
                {
                    if (addedPropFulfillsCompatabilityConstraint && !isAddedPropPartialStatic)
                    {
                        // std::cout << "Staging " << propTypeNames[propTypeId] << ": " << id << std::endl;
                        BicycleMango::stagingPropTuples[(*sunlambda_it)][propTypeId].push_back(id);
                    }
                }
            } // --- end sunlambda_it
        } // --- end typeset_it
    }

    // Props should only be removed by stage rather than considering type
    using PropRemovalSearch = std::function<bool(std::set<Stage>&)>;

    // Delay prop removal until the end of the current frame
    static inline std::unordered_map<PropTypeId, std::set<PropIdRaw>> propsToRemove;

    static inline std::unordered_map<SunLambda::Id, std::set<size_t>> tuplesToBreakup;

    static void RemoveProps(PropRemovalSearch Remove)
    {
        for (auto& propCategory : propTypeNames)
        {
            PropTypeId propTypeId = propCategory.first;
            for (auto& propData: ptpsq[propTypeId])
            {
                if (Remove(propData.second))
                {
                    propsToRemove[propTypeId].insert(propData.first);
                }
            }
        }
        for (auto& propTypeProps : propsToRemove)
        {
            // Find the Typesets that contain this proptypeid
            for (const Typeset& typeset : mappedPropTupleTypesets[propTypeProps.first])
            {
                size_t typesetRemovalIndex = 0;
                for (size_t i = 0; i < typeset.size(); i++)
                {
                    if (typeset[i] == propTypeProps.first)
                    {
                        typesetRemovalIndex = i;
                        break;
                    }
                }
                for (SunLambda::Id& sun : typesetSunLambdas[typeset])
                {
                    size_t index = 0;
                    for (auto& novelTuple : novelTuples[sun])
                    {
                        if (propTypeProps.second.count(novelTuple[typesetRemovalIndex].id))
                        {
                            tuplesToBreakup[sun].insert(index);
                        }
                        index++;
                    }
                }
            }
        }
    }

    static inline size_t GetTypesetIndex(SunLambda::Id sunid, PropTypeId ptid)
    {
        size_t i = 0;
        for (PropTypeId& seek_ptid : sunLambdaTypesets[sunid])
        {
            if (seek_ptid == ptid) return i;
            i++;
        }
        return i;
    }

    static void RemovePropsDelayed()
    {
        // TODO:
        for (auto& broken : tuplesToBreakup)
        {
            for (auto tuple_it = broken.second.rbegin(); tuple_it != broken.second.rend(); ++tuple_it)
            {
                // TODO: make this more efficient by swapping values to end and then erasing ending range
                auto tuple = novelTuples[broken.first].begin() + (*tuple_it);
                // Iterate through the tuple, checking and removing any partial static if it is contained in propsToRemove
                for (GlobalPropId& gpid : *tuple)
                {
                    bool shouldRemoveProp = propsToRemove.count(gpid.typeId) && propsToRemove[gpid.typeId].count(gpid.id);
                    if (shouldRemoveProp)
                    {
                        // Does this SunLambda have any partial static of this prop type
                        if (partialStatics.count(broken.first) && partialStatics[broken.first].count(gpid.typeId))
                        {
                            auto& pss = partialStatics[broken.first][gpid.typeId];
                            auto s_it = std::find(pss.begin(), pss.end(), gpid.id);
                            if (s_it != pss.end()) pss.erase(s_it);
                        }
                    } else
                    {
                        // If the prop is not removed, but the tuple is broken, we should restage it
                        // std::cout << "Restage prop of type " << propTypeNames[gpid.typeId] << " on SunLambda " << SunLambdaRegistry::GetInstance().Get(broken.first).name << std::endl;
                        BicycleMango::stagingPropTuples[broken.first][gpid.typeId].push_back(gpid.id);
                    }
                    
                }
                // TODO: Call breakups
                novelTuples[broken.first].erase(tuple);
            }
        }

        for (auto& propData : propsToRemove)
        {
            for (const PropIdRaw& propId : propData.second)
            {
                // Condense pool by reuse
                freeFunctions[propData.first](propId);
                // Make stages used by prop available
                for (auto& stage : ptpsq[propData.first][propId])
                {
                    instanceBuffer[stage.group].free(stage.instance);
                }
                ptpsq[propData.first].erase(propId);
            }
            if (ptpsq[propData.first].size() == 0)
            {
                ptpsq.erase(propData.first);
            }
        }
        
        propsToRemove.clear();
        tuplesToBreakup.clear();
    }

    // GetPropTypeId<PropType>
    // static void RemovePropsOfType()
    
    // Let's start over
    static void ResetProps()
    {
        ptpsq.clear();
        partialStatics.clear();
        stagingPropTuples.clear();
        novelTuples.clear();
        instanceBuffer.clear();
    }
    
    static void ResetSunLambdas()
    {
        // DO NOT CLEAR REGISTRY OR TYPESETS
        breakups.clear();
        emerges.clear();
        schedules.clear();
        novelTupleCreators.clear();
    }
    
    static void Reset()
    {
        ResetProps();
        ResetSunLambdas();
    }
    
    template <typename PropType>
    static void Singleton(SunLambda::Id sunLambdaId)
    {
        BicycleMango::novelTupleCreators[sunLambdaId].reuseOnStages[BicycleMango::GetPropTypeId<PropType>()] = 
                [](std::set<Stage>&, PropTypeId, std::set<Stage>&){ return true; };
    }

    template <typename PropType>
    static void Partial(SunLambda::Id sunLambdaId, std::function<bool(std::set<Stage>&, PropTypeId, std::set<Stage>&)> reuse)
    {
        BicycleMango::novelTupleCreators[sunLambdaId].reuseOnStages[BicycleMango::GetPropTypeId<PropType>()] = reuse;
    }

    template <typename PropType>
    static PropType& GetProp(PropId<PropType> id)
    {
        return GetProps[id.id];
    }

    template <typename PropType>
    static SparseArray<PropType>& GetProps()
    {
        static SparseArray<PropType> props;
        return props;
    }

    template <typename PropType>
    static PropType* GetProp()
    {
        for(auto& prop : GetProps<PropType>())
        {
            return &prop;
        }

        return nullptr;
    }

private:

    template <typename ... PTypes, std::size_t ... Is>
    static auto IterateProps(void (*functor)(PTypes...), const SunLambda::Id& id, std::index_sequence<Is...> seq)
    {
        for (auto& tuple : novelTuples[id])
        {
            functor(GetProps<std::decay_t<PTypes>>()[tuple[Is].id]...);
        }
    }

public:

    template <typename ... PTypes>
    static auto IterateProps(void (*functor)(PTypes...), const SunLambda::Id& id)
    {
        return IterateProps<PTypes...>(functor, id, std::index_sequence_for<PTypes...> {});
    }
};