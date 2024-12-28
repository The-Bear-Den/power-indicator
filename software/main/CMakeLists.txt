idf_component_register(SRCS "main.c"
                            "power.c"
                            "network.c"
                            "indicator.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES amber-dot-com-ca.pem)
