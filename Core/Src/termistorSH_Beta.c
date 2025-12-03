#include <math.h>
#define USE_STEINHART_HART (0)
#define USE_BETA (1)

#if USE_STEINHART_HART
	// Steinhart-Hart coefficients (example values for KY-013)
	#define SH_A 0.001129148 // Steinhart-Hart Equation coefficient A
	#define SH_B 0.000234125 // Steinhart-Hart Equation coefficient B
	#define SH_C 0.0000000876741 // Steinhart-Hart Equation coefficient C
#endif
#if USE_BETA
    // Beta Model parameters
    /* Calibrated Beta measured by user (from two-point measurement) */
    #define BETA 2742        // Beta (K) - user measured
    /* Nominal resistance and temperature for a 1 kΩ NTC at 25°C */
    #define R0 1000.0        // Resistance at T0 (ohms)
    #define T0 298.15        // Nominal Temperature (K) (25°C = 298.15K)
#endif

/* Fixed resistor in the divider (ohms). For your hardware this is 1k pull-down. */
#define DIVIDER_R 1000.0    // Fixed Resistor (ohms)

/**
 * @brief Converts the voltage from a voltage divider circuit to temperature in degrees Celsius.
 * 
 * This function uses the Steinhart-Hart model or the Beta model to calculate the temperature of a thermistor
 * based on the voltage measured from a voltage divider circuit.
 * 
 * Be careful when choosing the circuit voltage level. If 5000.0 millivolts (5.0 V) is used, bear in mind that the
 * voltage on the STM32F411 ADC Pin must not exceed 3300.0 millivolts (3.3 V). Thus, the current flowing on the
 * circuit should not exceed 330 uA. So, if the thermistor resistance becomes lower than 5151.52 ohms, the voltage on 
 * the ADC pin will be exceeded. For a 10 kohms NTC Thermistor, this represents a temperature higher than 105 oC.
 * 
 * @param Pin1Voltage_mV The supply voltage of the circuit (For the KY-103 is the Pin 1, which has a - sign), in millivolts.
 * @param readVoltage_mV The voltage over the fixed resistor (divider_R) read ((For the KY-103 is the Pin 2), in millivolts.
 * 
 * @return The temperature in degrees Celsius.
 */
float DividerVoltageToDegreesCelsius(float Pin1Voltage_mV, float readVoltage_mV)
{
    float thermistor_R, logThermistor_R; // Thermistor resistance and the natural logarithm of the thermistor resistance
    float tempThermistor;    

    // Input validation: Prevent division by zero or near-zero
    if (fabsf(readVoltage_mV) < 1e-6f) {
        return NAN;
    }
    // Calculate the thermistor resistance
    thermistor_R = (Pin1Voltage_mV * DIVIDER_R - DIVIDER_R * readVoltage_mV) / (readVoltage_mV);
#if USE_STEINHART_HART	
    // Calculate the natural logarithm of the thermistor resistance
    logThermistor_R = log(thermistor_R);    
    // Calculate the temperature in Kelvin using the Steinhart-Hart Equation
    tempThermistor = 1.0 / (SH_A + SH_B * logThermistor_R + SH_C * logThermistor_R * logThermistor_R * logThermistor_R);    
    // Convert Kelvin to Celsius and return
#endif
#if USE_BETA
	logThermistor_R = log(thermistor_R / R0);
	tempThermistor = 1.0 / ( (1.0 / T0) + (logThermistor_R / BETA) );
#endif
    return tempThermistor - 273.15;
}