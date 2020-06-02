#pragma once

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "envoy/common/exception.h"

#include "common/common/assert.h"
#include "common/common/fmt.h"

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"

namespace Envoy {
namespace Registry {

// Forward declaration of test class for friend declaration below.
template <typename T> class InjectFactory;

/**
 * General registry for implementation factories. The registry is templated by the Base class that a
 * set of factories conforms to.
 *
 * Classes are found by name, so a single name cannot be registered twice for the same Base class.
 * Factories are registered by reference and this reference is expected to be valid through the life
 * of the program. Factories cannot be deregistered.
 * Factories should generally be registered by statically instantiating the RegisterFactory class.
 *
 * Note: This class is not thread safe, so registration should only occur in a single threaded
 * environment, which is guaranteed by the static instantiation mentioned above.
 *
 * Example lookup: BaseFactoryType *factory =
 * FactoryRegistry<BaseFactoryType>::getFactory("example_factory_name");
 */
template <class Base> class FactoryRegistry {
public:
  /**
   * Return all registered factories in a comma delimited list.
   */
  static std::string allFactoryNames() {
    std::vector<absl::string_view> ret;
    ret.reserve(factories().size());
    for (const auto& factory : factories()) {
      ret.push_back(factory.first);
    }
    std::sort(ret.begin(), ret.end());

    return absl::StrJoin(ret, ",");
  }

  /**
   * Gets the current map of factory implementations.
   */
  static absl::flat_hash_map<std::string, Base*>& factories() {
    static auto* factories = new absl::flat_hash_map<std::string, Base*>;
    return *factories;
  }

  static void registerFactory(Base& factory, absl::string_view name) {
    auto result = factories().emplace(std::make_pair(name, &factory));
    if (!result.second) {
      throw EnvoyException(fmt::format("Double registration for name: '{}'", factory.name()));
    }
  }

  /**
   * Gets a factory by name. If the name isn't found in the registry, returns nullptr.
   */
  static Base* getFactory(absl::string_view name) {
    auto it = factories().find(name);
    if (it == factories().end()) {
      return nullptr;
    }
    return it->second;
  }

private:
  // Allow factory injection only in tests.
  friend class InjectFactory<Base>;

  /**
   * Replaces a factory by name. This method should only be used for testing purposes.
   * @param factory is the factory to inject.
   * @return Base* a pointer to the previously registered value.
   */
  static Base* replaceFactoryForTest(Base& factory) {
    auto it = factories().find(factory.name());
    Base* displaced = nullptr;
    if (it != factories().end()) {
      displaced = it->second;
      factories().erase(it);
    }

    factories().emplace(factory.name(), &factory);
    RELEASE_ASSERT(getFactory(factory.name()) == &factory, "");
    return displaced;
  }

  /**
   * Remove a factory by name. This method should only be used for testing purposes.
   * @param name is the name of the factory to remove.
   */
  static void removeFactoryForTest(absl::string_view name) {
    auto result = factories().erase(name);
    RELEASE_ASSERT(result == 1, "");
  }
};

/**
 * Factory registration template. Enables users to register a particular implementation factory with
 * the FactoryRegistry by instantiating this templated class with the specific factory class and the
 * general Base class to which that factory conforms.
 *
 * Because factories are generally registered once and live for the length of the program, the
 * standard use of this class is static instantiation within a linked implementation's translation
 * unit. For an example of a typical use case, @see NamedNetworkFilterConfigFactory.
 *
 * Example registration: REGISTER_FACTORY(SpecificFactory, BaseFactory);
 *                       REGISTER_FACTORY(SpecificFactory, BaseFactory){"deprecated_name"};
 */
template <class T, class Base> class RegisterFactory {
public:
  /**
   * Constructor that registers an instance of the factory with the FactoryRegistry.
   */
  RegisterFactory() {
    ASSERT(!instance_.name().empty());
    FactoryRegistry<Base>::registerFactory(instance_, instance_.name());
  }

  /**
   * Constructor that registers an instance of the factory with the FactoryRegistry along with
   * deprecated names.
   */
  explicit RegisterFactory(std::initializer_list<absl::string_view> deprecated_names) {
    if (!instance_.name().empty()) {
      FactoryRegistry<Base>::registerFactory(instance_, instance_.name());
    } else {
      ASSERT(deprecated_names.size() != 0);
    }
    for (auto deprecated_name : deprecated_names) {
      ASSERT(!deprecated_name.empty());
      FactoryRegistry<Base>::registerFactory(instance_, deprecated_name);
    }
  }

private:
  T instance_{};
};

/**
 * Macro used for static registration.
 */
#define REGISTER_FACTORY(FACTORY, BASE)                                                            \
  ABSL_ATTRIBUTE_UNUSED void forceRegister##FACTORY() {}                                           \
  static Envoy::Registry::RegisterFactory</* NOLINT(fuchsia-statically-constructed-objects) */     \
                                          FACTORY, BASE>                                           \
      FACTORY##_registered

/**
 * Macro used for static registration declaration.
 * Calling forceRegister...(); can be used to force the static factory initializer to run in a
 * setting in which Envoy is bundled as a static archive. In this case, the static initializer is
 * not run until a function in the compilation unit is invoked. The force function can be invoked
 * from a static library wrapper.
 */
#define DECLARE_FACTORY(FACTORY) ABSL_ATTRIBUTE_UNUSED void forceRegister##FACTORY()

} // namespace Registry
} // namespace Envoy