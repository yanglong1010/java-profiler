package com.datadoghq.profiler.stresstest;

import org.openjdk.jmh.annotations.Param;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.State;

@State(Scope.Benchmark)
public class Configuration {

    public static final String BASE_COMMAND = "cpu=1us,wall=1us";

    @Param({BASE_COMMAND})
    public String command;
}
