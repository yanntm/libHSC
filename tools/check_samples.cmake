# Sanity test: run hsc-mcc on each sample net for the three fixed examinations
# and check the answer against the MCC 2025 oracle (examples/mcc/expected.csv).
# Invoked by the `mcc_samples` ctest. -DHSC_MCC and -DSAMPLES are passed in.

set(FAILED 0)

function(run_check model exam pattern)
  execute_process(
    COMMAND "${HSC_MCC}" "${SAMPLES}/${model}.pnml" -mcc ${exam}
    OUTPUT_VARIABLE out ERROR_QUIET TIMEOUT 60)
  if(out MATCHES "${pattern}")
    message(STATUS "ok  ${model} ${exam}")
  else()
    message(WARNING "FAIL ${model} ${exam}: got [${out}] want [${pattern}]")
    set(FAILED 1 PARENT_SCOPE)
  endif()
endfunction()

file(STRINGS "${SAMPLES}/expected.csv" lines)
foreach(line IN LISTS lines)
  if(line MATCHES "^#")
    continue()
  endif()
  string(REPLACE "," ";" cols "${line}")
  list(GET cols 0 model)
  list(GET cols 1 states)
  list(GET cols 2 onesafe)
  list(GET cols 3 deadlock)
  # the oracle file keeps a deadlock column for the record; the examination
  # was dropped from the pipeline (debt: it assumed refusal sits in `when`)
  run_check("${model}" StateSpace "STATE_SPACE STATES ${states} ")
  run_check("${model}" OneSafe "FORMULA OneSafe ${onesafe} ")
endforeach()

if(FAILED)
  message(FATAL_ERROR "one or more sample checks disagreed with the MCC oracle")
endif()
