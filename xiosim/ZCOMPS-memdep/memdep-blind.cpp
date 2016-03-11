/* memdep-blind.cpp: Always predict that no memory dependencies/conflicts exist */
/*
 * __COPYRIGHT__ GT
 */
#define COMPONENT_NAME "blind"

#ifdef MEMDEP_PARSE_ARGS
if(!strcasecmp(COMPONENT_NAME,type))
{
  return std::make_unique<memdep_blind_t>(core);
}
#else

class memdep_blind_t: public memdep_t
{
  public:
  /* CONSTRUCTOR */
  memdep_blind_t(const struct core_t * _core) : memdep_t(_core)
  {
    init();
    name = COMPONENT_NAME;
    type = COMPONENT_NAME;
  }
  /* LOOKUP */
  MEMDEP_LOOKUP_HEADER
  {
    MEMDEP_STAT(lookups++;)
    return true;
  }
};

#endif /* MEMDEP_PARSE_ARGS */
#undef COMPONENT_NAME
