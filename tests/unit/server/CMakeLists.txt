target_sources(
    canary_ut
    PRIVATE signals_test.cpp
            network/message/networkmessage_test.cpp
            network/protocol/multiprotocol_test.cpp
)
