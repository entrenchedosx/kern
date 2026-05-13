/* *
 * kern/runtime/core/module_registry.inl - Template Implementation
 */

// Implementation of static factory registration method
// This is included after the class definition but within the namespace
void kern::runtime::ModuleRegistry::registerFactory(const std::string& name, ModuleFactory factory) {
    auto& registry = getFactoryRegistry();
    registry[name] = std::move(factory);
}
