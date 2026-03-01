module;

export module charm.core.factory;

#if defined(CHARM_VIVID_PROFILE_FULL)
export import charm.core.factory.full;
#else
export import charm.core.factory.basic;
#endif
