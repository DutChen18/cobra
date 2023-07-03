#ifndef COBRA_TEST_ASSERT_HH
#define COBRA_TEST_ASSERT_HH

#include <cassert>

#define ASSERT_THROW(statement, exception)                                     \
    try {                                                                      \
        statement;                                                             \
        assert(0 && "did not throw");                                          \
    } catch (const exception &ex) {                                            \
        (void)ex;                                                              \
    } catch (...) {                                                            \
        assert(0 && "threw something else");                                   \
    }


#endif
