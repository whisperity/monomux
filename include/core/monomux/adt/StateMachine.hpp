/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <cassert>
#include <functional>
#include <optional>
#include <utility>

#include <iostream>

#include "monomux/adt/FunctionExtras.hpp"
#include "monomux/adt/Metaprogramming.hpp"

namespace monomux::state_machine
{

namespace detail
{

/// Exhibits a \e Callable that does absolutely nothing.
struct NoOp
{
  template <typename... Ts>
  inline constexpr void operator()(Ts&&... /**/) const noexcept
  {}
};

/// Provides the data member \p State if \p B is \p true.
template <typename T, bool B = true> struct ConditionalUserStateMember
{
  /// The run-time extra state of the state machine which can be queried and
  /// mutated with callback functions associated with core state transitions.
  T UserState;

  /// \returns the current state.
  [[nodiscard]] const T& getState() const noexcept { return UserState; }
  /// \returns the current state.
  MONOMUX_MEMBER_0(T&, getState, [[nodiscard]], noexcept);
};
template <typename T> struct ConditionalUserStateMember<T, false>
{};

/// Provides the data member \p UserCallback if \p B is \p true.
template <class StateType, bool B = true> struct ConditionalUserCallbackMember
{
  using CallbackType = std::function<void(StateType&)>;
  /// The callback function which receives the mutable \p UserState and might
  /// execute user-specific business logic.
  CallbackType UserCallback;

  inline constexpr ConditionalUserCallbackMember() : UserCallback({}) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  inline constexpr ConditionalUserCallbackMember(CallbackType&& Fn)
    : UserCallback(Fn)
  {}

  [[nodiscard]] const CallbackType& getCallback() const noexcept
  {
    return UserCallback;
  }
};
template <class StateType>
struct ConditionalUserCallbackMember<StateType, false>
{
  using CallbackType = void;
};

/// Represents a transition object at run-time.
///
/// \note Not to be confused with \p detail::Base<...>::Transition!
template <class UserState>
class Transition
  : private ConditionalUserCallbackMember<UserState, !std::is_void_v<UserState>>
{
  static inline constexpr const bool CanHaveCallback =
    !std::is_void_v<UserState>;
  using CallbackWrapper =
    ConditionalUserCallbackMember<UserState, CanHaveCallback>;

  meta::index_t TargetStateIndex{};

public:
  /// Initialises an empty \p Transition.
  ///
  /// This function only participates in overload resolution if the state
  /// machine does not have a user-defined state, and as such, can not have
  /// callbacks.
  template <bool Enable = !CanHaveCallback,
            std::enable_if_t<Enable, bool> = false>
  inline constexpr Transition()
    : TargetStateIndex(static_cast<meta::index_t>(0))
  {}

  /// Initialises an empty \p Transition without a callback.
  ///
  /// This function only participates in overload resolution if the state
  /// machine has a user-defined state.
  template <bool Enable = CanHaveCallback,
            class CallbackType =
              std::enable_if_t<Enable, typename CallbackWrapper::CallbackType>>
  inline constexpr Transition()
    : ConditionalUserCallbackMember<UserState, Enable>(),
      TargetStateIndex(static_cast<meta::index_t>(0))
  {}

  /// Initialises a \p Transition without a callback.
  ///
  /// This function only participates in overload resolution if the state
  /// machine does not have a user-defined state, and as such, can not have
  /// callbacks.
  template <bool Enable = !CanHaveCallback,
            std::enable_if_t<Enable, bool> = false>
  explicit inline constexpr Transition(meta::index_t TargetIndex)
    : TargetStateIndex(TargetIndex)
  {}

  /// Initialises a \p Transition with a callback.
  ///
  /// This function only participates in overload resolution if the state
  /// machine has a user-defined state. The callback \b MUST be a function
  /// that takes the \p UserState as a mutable reference.
  template <bool Enable = CanHaveCallback,
            class CallbackType =
              std::enable_if_t<Enable,
                               typename CallbackWrapper::CallbackType>>
  inline constexpr Transition( // NOLINT(google-explicit-constructor)
    meta::index_t TargetIndex,
    CallbackType&& Fn)
    : ConditionalUserCallbackMember<UserState, Enable>(
        std::forward<CallbackType>(Fn)),
      TargetStateIndex(TargetIndex)
  {}

  /// Initialises a \p Transition with an empty callback.
  ///
  /// This function only participates in overload resolution if the state
  /// machine has a user-defined state.
  template <bool Enable = CanHaveCallback,
            class CallbackType =
              std::enable_if_t<Enable, typename CallbackWrapper::CallbackType>>
  explicit inline constexpr Transition(meta::index_t TargetIndex)
    : ConditionalUserCallbackMember<UserState, Enable>(),
      TargetStateIndex(TargetIndex)
  {}

  [[nodiscard]] bool leadsAnywhere() const noexcept
  {
    return TargetStateIndex != static_cast<meta::index_t>(0);
  }

  [[nodiscard]] meta::index_t getTarget() const noexcept
  {
    return TargetStateIndex;
  }

  [[nodiscard]] std::
    conditional_t<CanHaveCallback, typename CallbackWrapper::CallbackType, NoOp>
    getCallback() const noexcept
  {
    if constexpr (CanHaveCallback)
      return this->UserCallback;
    else
      return NoOp{};
  }
};

/// Provides base methods for state machines which have \p EdgeLabel values as
/// their transition labels and optionally contain a user \p State at runtime.
template <class EdgeLabel, class UserState = void>
struct Base
  : public ConditionalUserStateMember<UserState, !std::is_void_v<UserState>>
{
  using Index = meta::index_t;
  using EdgeType = EdgeLabel;
  using UserStateType = UserState;
  static inline constexpr const bool CanHaveCallbacks =
    !std::is_void_v<UserState>;

  /// A core state of the state machine. States are identified by a numeric
  /// index. The state machine being in a particular state is its main property.
  ///
  /// \tparam DefaultTransitionTargetIndex The index of the \p State where the
  /// state machine should translate if receiving an input that does not exist
  /// as an outgoing edge. If \p 0, the default traversal is disabled.
  template <Index I, Index DefaultTransitionTargetIndex = 0> struct State
  {
    static inline constexpr const auto Index = I;
    static inline constexpr const auto DefaultTransitionTarget =
      DefaultTransitionTargetIndex;

    static inline constexpr const bool HasDefaultTransition =
      DefaultTransitionTarget != 0;
  };

  /// A transition of the state machine from \p From to the \p To state, if
  /// it receives \p Value as input while in the \p From state.
  ///
  /// Optionally, a \p Callback might be executed when the transition is
  /// performed. This \p Callback receives the current \p UserState as a
  /// mutable input.
  template <Index FromState,
            EdgeType Value,
            Index ToState,
            class CallbackType = void>
  struct Transition
  {
    static constexpr const bool CanHaveCallback = !std::is_void_v<UserState>;
    static inline constexpr const auto From = FromState;
    static inline constexpr const auto To = ToState;
    static inline constexpr const auto EdgeValue = Value;

    using Callback = CallbackType;
    static_assert(
      CanHaveCallback || std::is_same_v<Callback, void>,
      "Callback cannot be specified if there is no user-specified state");
  };

  template <Index FromState, EdgeType Value, Index ToState>
  static inline constexpr auto transition()
  {
    return Transition<FromState, Value, ToState>{};
  }

  template <Index FromState, EdgeType Value, Index ToState, class CallbackType>
  static inline constexpr auto transition(CallbackType&& /*Callback*/)
  {
    return Transition<FromState, Value, ToState, CallbackType>{};
  }
};

/// Provides the compile-time configuration of the machine. This is its list of
/// states and list of recorded transitions.
template <class EdgeLabel,
          class StateList,
          class TransitionList,
          class UserState,
          class StateCallbackList>
struct Configuration : public Base<EdgeLabel, UserState>
{
  using BaseT = Base<EdgeLabel, UserState>;
  using Index = typename BaseT::Index;
  using EdgeType = typename BaseT::EdgeType;
  using States = StateList;
  using Transitions = TransitionList;
  static inline constexpr const bool CanHaveCallbacks =
    !std::is_void_v<UserState>;
  using StateCallbacks =
    std::conditional_t<CanHaveCallbacks, StateCallbackList, meta::empty_list>;

  static inline constexpr const Index StateCount = meta::size_v<States>;

  /// Generates a new \p StateList and \p TransitionList of a state machine
  /// by adding a transition.
  ///
  /// \tparam FromStateIdx The \p FromState of the \p Transition
  /// \tparam Edge         The edge label of the \p Transition.
  /// \tparam NeedsAddingTransition If \p true, the \p Transition will be added,
  /// otherwise the state machine will not be mutated.
  /// \tparam ToStateIdx   The \p ToState of the \p Transition. If \p -1,
  /// a new state will be automatically created.
  ///
  /// \note This is a \b helper definition. It does \b NOT check whether the
  /// resulting configuration is valid.
  template <Index FromStateIdx,
            EdgeLabel Edge,
            bool NeedsAddingTheTransition,
            Index ToStateIdx = static_cast<Index>(-1)>
  struct NewStatesAndTransitions
  {
    static inline constexpr const bool NeedsNewState = (ToStateIdx == -1);
    static inline constexpr const Index TargetStateIndex =
      NeedsAddingTheTransition
        ? (NeedsNewState ? (meta::size_v<StateList> + 1) : ToStateIdx)
        : ToStateIdx;

    using NewStateList = std::conditional_t<
      NeedsNewState,
      meta::append_t<typename BaseT::template State<TargetStateIndex>, States>,
      States>;

    using NewTransitionList = std::conditional_t<
      NeedsAddingTheTransition,
      meta::append_t<typename BaseT::template Transition<FromStateIdx,
                                                         Edge,
                                                         TargetStateIndex>,
                     Transitions>,
      Transitions>;

    static inline constexpr const Index NewTransitionIndex =
      (NeedsAddingTheTransition ? meta::size_v<NewTransitionList> : -1);

    using NewStateCallbackList =
      std::conditional_t<NeedsNewState,
                         std::conditional_t<CanHaveCallbacks,
                                            meta::append_t<void, States>,
                                            meta::empty_list>,
                         States>;
  };
};

template <meta::index_t FromState, class EdgeType, EdgeType EdgeValue>
struct TransitionAccessFactory
{
  template <class Transition> struct Fn
  {
    // NOLINTNEXTLINE(readability-identifier-naming): STL compatibility.
    static constexpr const bool value =
      Transition::From == FromState && Transition::EdgeValue == EdgeValue;
  };
};

/// Provides meta-methods to change the \p Config of a machine.
/// The \p Mutator always have a \e "current" state index to which the mutations
/// are applied.
template <meta::index_t CurrentMutatedStateIdx, class Config>
struct Mutator : public Config
{
  using Base = typename Config::Base;
  using Index = typename Base::Index;
  using EdgeType = typename Base::EdgeType;
  using Configuration = Config;
  static inline constexpr const bool CanHaveCallbacks =
    Config::CanHaveCallbacks;

  static inline constexpr const Index CurrentStateIndex =
    CurrentMutatedStateIdx;
  using CurrentMutatedState =
    typename meta::access_t<CurrentStateIndex, typename Config::States>;

  /// Switch the current \p Mutator over to allow the mutation of \p StateIdx
  /// instead.
  template <Index StateIdx>
  inline constexpr auto switchToState() -> Mutator<StateIdx, Config>
  {
    return {};
  }

  inline constexpr auto addNewState()
  {
    using NewStateList =
      meta::append_t<typename Configuration::BaseT::template State<
                       meta::size_v<typename Configuration::States>>,
                     typename Configuration::States>;
    using NewStateCallbackList = std::conditional_t<
      CanHaveCallbacks,
      meta::append_t<void, typename Configuration::StateCallbacks>,
      meta::empty_list>;

    using NewConfig = typename detail::Configuration<
      EdgeType,
      NewStateList,
      typename Configuration::Transitions,
      typename Configuration::BaseT::UserStateType,
      NewStateCallbackList>;
    return Mutator<CurrentStateIndex, NewConfig>{};
  }

  template <Index DefaultTransitionTarget>
  inline constexpr auto setDefaultTransitionTarget()
  {
    using NewConfig = typename detail::Configuration<
      EdgeType,
      meta::replace_t<
        typename Configuration::States,
        CurrentStateIndex,
        typename Configuration::BaseT::template State<CurrentStateIndex,
                                                      DefaultTransitionTarget>>,
      typename Configuration::Transitions,
      typename Configuration::BaseT::UserStateType,
      typename Configuration::StateCallbacks>;
    return Mutator<CurrentStateIndex, NewConfig>{};
  }

  /// Switches the current \p Mutator by traversing from the \b Current state
  /// the transition that has the given \p EdgeValue. If such a transition does
  /// \e not exist, a new state is created and to it, a transition is added.
  /// If it already exists, only the traversal is performed. Nevertheless,
  /// returns a \p Mutator which mutates the result of the transition.
  template <EdgeType EdgeValue> inline constexpr auto addOrTraverseTransition()
  {
    using AlreadyExistingTransition =
      meta::find_t<TransitionAccessFactory<CurrentStateIndex,
                                           EdgeType,
                                           EdgeValue>::template Fn,
                   typename Configuration::Transitions>;

    if constexpr (!AlreadyExistingTransition::value)
    {
      using NewLists = typename Configuration::template NewStatesAndTransitions<
        CurrentStateIndex,
        EdgeValue,
        !AlreadyExistingTransition::value>;
      using NewConfig = typename detail::Configuration<
        EdgeType,
        typename NewLists::NewStateList,
        typename NewLists::NewTransitionList,
        typename Configuration::BaseT::UserStateType,
        typename NewLists::NewStateCallbackList>;

      return Mutator<NewLists::TargetStateIndex, NewConfig>{};
    }
    else
      return Mutator<AlreadyExistingTransition::type::To, Configuration>{};
  }

  /// Switches the current \p Mutator by traversing from the \b Current state
  /// the transition that has the given \p EdgeValue. If such a transition does
  /// \e not exist, a new state is created and to it, a transition is added.
  /// If it already exists, only the traversal is performed. Nevertheless,
  /// returns a \p Mutator which mutates the result of the transition.
  template <EdgeType EdgeValue, class CallbackType>
  inline constexpr auto addOrTraverseTransition(CallbackType&& /*Callback*/)
  {
    static_assert(
      CanHaveCallbacks || std::is_same_v<CallbackType, void>,
      "Callback cannot be specified if there is no user-specified state");
    using AlreadyExistingTransition =
      meta::find_t<TransitionAccessFactory<CurrentStateIndex,
                                           EdgeType,
                                           EdgeValue>::template Fn,
                   typename Configuration::Transitions>;

    if constexpr (!AlreadyExistingTransition::value)
    {
      using NewLists = typename Configuration::template NewStatesAndTransitions<
        CurrentStateIndex,
        EdgeValue,
        !AlreadyExistingTransition::value,
        CallbackType>;
      using NewConfig = typename detail::Configuration<
        EdgeType,
        typename NewLists::NewStateList,
        typename NewLists::NewTransitionList,
        typename Configuration::BaseT::UserStateType,
        typename NewLists::NewStateCallbackList>;

      return Mutator<NewLists::TargetStateIndex, NewConfig>{};
    }
    else
      return Mutator<AlreadyExistingTransition::type::To, Configuration>{};
  }

  /// Switches the current \p Mutator by adding (or traversing, if already
  /// exists) from the \b Current state via the transition that has the given
  /// \p EdgeValue to the \p ToState. Nevertheless, returns a \p Mutator which
  /// mutates the result of the transition.
  template <EdgeType EdgeValue, Index ToStateIdx>
  inline constexpr auto addTransition()
  {
    using AlreadyExistingTransition =
      meta::find_t<TransitionAccessFactory<CurrentStateIndex,
                                           EdgeType,
                                           EdgeValue>::template Fn,
                   typename Configuration::Transitions>;

    if constexpr (!AlreadyExistingTransition::value)
    {
      using NewLists = typename Configuration::template NewStatesAndTransitions<
        CurrentStateIndex,
        EdgeValue,
        !AlreadyExistingTransition::value,
        ToStateIdx>;
      using NewConfig = typename detail::Configuration<
        EdgeType,
        typename NewLists::NewStateList,
        typename NewLists::NewTransitionList,
        typename Configuration::BaseT::UserStateType,
        typename NewLists::NewStateCallbackList>;

      return Mutator<NewLists::TargetStateIndex, NewConfig>{};
    }
    else
      return Mutator<AlreadyExistingTransition::type::To, Configuration>{};
  }
};

template <class EdgeLabel, class UserState = void>
static inline constexpr auto createMachine() -> Mutator<
  1, // Start mutating from the start state (#1).
  Configuration<
    EdgeLabel,
    // Give the start state index 1.
    meta::list<typename Base<EdgeLabel, UserState>::template State<1>>,
    meta::empty_list,
    UserState,
    // If there is a possibility for a callback function, store that the
    // start state does not have a callback function.
    std::conditional_t<Base<EdgeLabel, UserState>::CanHaveCallbacks,
                       meta::list<void>,
                       meta::empty_list>>>
{
  return {};
}

template <class EdgeType> struct TransitionToEdgeValueFactory
{
  template <class Transition> struct Fn
  {
    // NOLINTNEXTLINE(readability-identifier-naming): STL compatibility.
    using type = std::integral_constant<meta::index_t, Transition::EdgeValue>;
  };
};

template <meta::index_t StateIdx> struct TransitionFromStateFactory
{
  template <class Transition> struct Fn
  {
    // NOLINTNEXTLINE(readability-identifier-naming): STL compatibility.
    static constexpr const bool value = Transition::From == StateIdx;
  };
};

template <class EdgeType, EdgeType EdgeValue>
struct TransitionWithEdgeValueFactory
{
  template <class Transition> struct Fn
  {
    // NOLINTNEXTLINE(readability-identifier-naming): STL compatibility.
    static constexpr const bool value = Transition::EdgeValue == EdgeValue;
  };
};

template <class UserState, std::size_t LookupTableSize>
using LookupVectorType = std::array<Transition<UserState>, LookupTableSize>;

template <class UserState, std::size_t LookupTableSize, std::size_t StateCount>
using LookupTableType =
  std::array<LookupVectorType<UserState, LookupTableSize>, StateCount>;

/// Builds a compile-time known-size buffer (in the form of a \p std::array)
/// for the transitions which survived the filtering.
template <class EdgeType,
          EdgeType LowestBoundValue,
          std::size_t ArraySize,
          class UserState,
          class FilteredTransitionList>
struct TransitionLookupArrayBuilder
{
private:
  template <EdgeType EdgeValue>
  using TransitionWithEdgeValue = meta::access_or_default_t<
    1,
    NoOp,
    meta::filter_t<
      TransitionWithEdgeValueFactory<EdgeType, EdgeValue>::template Fn,
      FilteredTransitionList>>;

  template <class MetaTransition>
  static constexpr Transition<UserState>
  toRuntimeTransition(MetaTransition /*T*/)
  {
    if constexpr (std::is_same_v<MetaTransition, NoOp>)
      return Transition<UserState>{};
    else
    {
      if constexpr (!MetaTransition::CanHaveCallback)
        return Transition<UserState>{MetaTransition::To};
      else
        return Transition<UserState>{MetaTransition::To,
                                     MetaTransition::Callback};
    }
  }

  template <EdgeType... Vs>
  static constexpr auto getArray(std::integer_sequence<EdgeType, Vs...> /*Seq*/)
    -> LookupVectorType<UserState, ArraySize>
  {
    return {
      toRuntimeTransition(TransitionWithEdgeValue<LowestBoundValue + Vs>{})...};
  }

public:
  static constexpr const auto Array =
    getArray(std::make_integer_sequence<EdgeType, ArraySize>{});
};

/// Helps grouping the \p TransitionList into a \p List of \p Transitions for
/// each state that transitions eminate from.
template <class EdgeType,
          EdgeType LowestBoundValue,
          std::size_t ExpectedOutputLength,
          class UserState,
          class TransitionList>
struct TransitionLookupArrayBuilderFactory
{
  /// Extracts and converts the transitions associated with the given
  /// \p FromStateIntegralConstant, expressed as an \p std::integral_constant
  /// so this \p Fn is passable to \p meta::map_t.
  template <class FromStateIntegralConstant> struct Fn
  {
  private:
    using TransitionsFromCurrentState = meta::filter_t<
      TransitionFromStateFactory<FromStateIntegralConstant::value>::template Fn,
      TransitionList>;

  public:
    using type = TransitionLookupArrayBuilder<EdgeType,
                                              LowestBoundValue,
                                              ExpectedOutputLength,
                                              UserState,
                                              TransitionsFromCurrentState>;
  };
};

/// Creates an array of arrays which contain, for each state indexed, the
/// out-going transitions that was prepared by \p TransitionLookupArrayBuilder.
template <class UserState,
          std::size_t InnerArraySize,
          class ListOfTransitionArrayBuilders,
          meta::index_t... StateIndices>
static constexpr LookupTableType<UserState,
                                 InnerArraySize,
                                 sizeof...(StateIndices)>
toTransitionArrayOfArrays(
  std::integer_sequence<meta::index_t, StateIndices...> /*Seq*/)
{
  return {
    meta::access_t<StateIndices + 1, ListOfTransitionArrayBuilders>::Array...};
}

template <class MetaState> struct StateToDefaultTransition
{
  using type =
    std::integral_constant<meta::index_t, MetaState::DefaultTransitionTarget>;
};

template <class State, std::size_t StateCount>
using CallbackVectorType =
  std::array<std::conditional_t<!std::is_void_v<State>,
                                typename detail::ConditionalUserCallbackMember<
                                  State>::CallbackType,
                                char>,
             !std::is_void_v<State> ? StateCount : 1>;

} // namespace detail

/// A run-time evaluateable data structure representing a state machine,
/// compiled from a compile-time metastructure.
///
/// \tparam EdgeType The type of the input values when performing edge
/// transitions.
/// \tparam State The user-defined state type inside the machine which may be
/// mutated by transitions.
template <class EdgeType,
          EdgeType LowestBoundValue,
          std::size_t StateCount,
          std::size_t LookupTableSize,
          class State = void>
class Machine
  : private detail::ConditionalUserStateMember<State, !std::is_void_v<State>>
{
  static constexpr const bool HasState = !std::is_void_v<State>;
  static constexpr const std::size_t NumStates = StateCount;

  using Transition = detail::Transition<State>;

  const bool InvalidInputKeepsState = false;
  /// Holds the lookup table for all states, first indexed by the current state
  /// leading to the transition objects.
  const detail::LookupTableType<State, LookupTableSize, StateCount> Transitions;

  /// Holds the default transition target for each state (at the index in the
  /// array), or \p 0 if there is no defualt transition targets.
  const std::array<meta::index_t, StateCount> DefaultTargets;

  using CallbackVectorType = detail::CallbackVectorType<State, StateCount>;
  /// Holds the list of \p Callbacks that are fired when the state machine
  /// reaches a \p State as indexed in this array, if a user-defined state
  /// exists.
  const CallbackVectorType Callbacks;

  meta::index_t CurrentState = 0;
  bool Errored = false;

public:
  constexpr Machine(
    detail::LookupTableType<State, LookupTableSize, StateCount> Transitions,
    std::array<meta::index_t, StateCount> DefaultTransitions,
    CallbackVectorType StateCallbacks,
    bool InvalidInputKeepsState)
    : InvalidInputKeepsState(InvalidInputKeepsState),
      Transitions(std::move(Transitions)),
      DefaultTargets(std::move(DefaultTransitions)),
      Callbacks(std::move(StateCallbacks))
  {
    reset();
  }

  /// \returns the numeric identifier of the current state of the state machine.
  [[nodiscard]] meta::index_t getCurrentState() const noexcept
  {
    return CurrentState;
  }

  /// \returns if the machine reached an error state.
  [[nodiscard]] bool hasErrored() const noexcept { return Errored; }

  /// Resets the state machine to the start state, and resets the contents of
  /// the user-specified \p State if such exists.
  void reset()
  {
    if constexpr (HasState)
      this->UserState = State{};
    CurrentState = StateCount > 0 ? 1 : 0;
    Errored = StateCount == 0;
  }

  /// Performs a state transition based on the input edge value specified.
  Machine& operator()(EdgeType&& Input) noexcept
  {
    if (CurrentState == 0 || Errored)
      return *this;

    const meta::index_t StateAsLookupIdx = CurrentState - 1;
    const auto& TransitionsFromCurrentState = Transitions.at(StateAsLookupIdx);

    // Translate the input value to its offset in the lookup table. The lookup
    // table was constructed only for the range of edge values where actual
    // meaningful transitions are performed. Put simply, map [X, Y, ..., Z] to
    // [0, 1, ..., 2] if X is the "smallest" edge value ever specified in the
    // machine's template.
    const meta::index_t Offset = Input - LowestBoundValue;
    if (Offset >= LookupTableSize)
    {
      if (const auto DefTgt = DefaultTargets.at(StateAsLookupIdx); DefTgt != 0)
        CurrentState = DefTgt;
      else if (!InvalidInputKeepsState)
        Errored = true;
      return *this;
    }

    const Transition& SelectedTransition =
      TransitionsFromCurrentState.at(Offset);
    if (!SelectedTransition.leadsAnywhere())
    {
      if (const auto DefTgt = DefaultTargets.at(StateAsLookupIdx); DefTgt != 0)
        CurrentState = DefTgt;
      else if (!InvalidInputKeepsState)
        Errored = true;
      return *this;
    }

    CurrentState = SelectedTransition.getTarget();
    if constexpr (HasState)
    {
      SelectedTransition.getCallback()(this->getState());
    }
    return *this;
  }
};

/// Compiles the specified in-progress state machine into a run-time executable
/// data structure.
template <typename Mutator>
static inline constexpr auto compile(Mutator /*M*/,
                                     bool InvalidInputKeepsState = false)
{
  using namespace monomux::meta;
  using Index = typename Mutator::Index;
  using Configuration = typename Mutator::Configuration;
  using EdgeType = typename Configuration::BaseT::EdgeType;
  using StateType = typename Configuration::BaseT::UserStateType;

  // First, we need to calculate how big the interval of transition *inputs*
  // are, as this is required to have a fixed-size lookup table.
  using TransitionValues =
    map_t<detail::TransitionToEdgeValueFactory<EdgeType>::template Fn,
          typename Configuration::Transitions>;
  constexpr const Index MinTransitionEdgeValue = min_v<TransitionValues>;
  constexpr const Index MaxTransitionEdgeValue = max_v<TransitionValues>;
  constexpr const std::size_t Range =
    MaxTransitionEdgeValue - MinTransitionEdgeValue + 1;

  // Second, we build the transition edges from each state to their targets,
  // where one exists.
  using TransitionMap =
    map_t<detail::TransitionLookupArrayBuilderFactory<
            EdgeType,
            MinTransitionEdgeValue,
            Range,
            StateType,
            typename Configuration::Transitions>::template Fn,
          // Drop the index 0 as it is not used, the \p List indexes from 1.
          tail_t<make_integral_constants_t<
            std::make_integer_sequence<Index, Configuration::StateCount + 1>>>>;
  static_assert(size_v<TransitionMap> == Configuration::StateCount);

  constexpr const auto LookupArray =
    detail::toTransitionArrayOfArrays<StateType, Range, TransitionMap>(
      std::make_integer_sequence<Index, Configuration::StateCount>{});

  using ProjectedStateDefaultTransitions =
    map_t<detail::StateToDefaultTransition, typename Configuration::States>;

  using StateCallbackVectorType =
    detail::CallbackVectorType<StateType, Configuration::StateCount>;
  if constexpr (Configuration::CanHaveCallbacks) {}

  return Machine<EdgeType,
                 MinTransitionEdgeValue,
                 Configuration::StateCount,
                 Range,
                 StateType>(
    LookupArray,
    meta::integral_constants_to_array<ProjectedStateDefaultTransitions>(),
    InvalidInputKeepsState);
}

/// Creates the \b compile-time metastructure for a new state machine that
/// will accept \p EdgeLabel values as the transition inputs and \e optionally
/// contain a run-time \p UserState of the specified type.
template <class EdgeLabel, class UserState = void>
static inline constexpr auto createMachine()
{
  return detail::createMachine<EdgeLabel, UserState>();
}

} // namespace monomux::state_machine
