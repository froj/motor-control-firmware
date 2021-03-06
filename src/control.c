#include <ch.h>
#include <hal.h>
#include <math.h>
#include <pid/pid.h>
#include "motor_pwm.h"
#include "analog.h"
#include "encoder.h"
#include "parameter/parameter.h"
#include "main.h"
#include "pid_cascade.h"
#include "timestamp/timestamp.h"
#include "filter/basic.h"
#include "motor_protection.h"
#include "feedback.h"
#include "setpoint.h"

#include "control.h"

#define LOW_BATT_TH 12.f // [V]


struct pid_param_s {
    parameter_t kp;
    parameter_t ki;
    parameter_t kd;
    parameter_t i_limit;
};



struct feedback_s control_feedback;
motor_protection_t control_motor_protection;

binary_semaphore_t setpoint_interpolation_lock;
static setpoint_interpolator_t setpoint_interpolation;
static struct pid_cascade_s ctrl;

// control loop parameters
static parameter_namespace_t param_ns_control;
static parameter_t param_low_batt_th;
static parameter_t param_vel_limit;
static parameter_t param_torque_limit;
static parameter_t param_acc_limit;
static parameter_namespace_t param_ns_pos_ctrl;
static parameter_namespace_t param_ns_vel_ctrl;
static parameter_namespace_t param_ns_cur_ctrl;
static struct pid_param_s pos_pid_params;
static struct pid_param_s vel_pid_params;
static struct pid_param_s cur_pid_params;
static parameter_namespace_t param_ns_motor;
static parameter_t param_torque_cst;
static parameter_namespace_t param_ns_thermal;
static parameter_t param_current_gain;
static parameter_t param_max_temp;
static parameter_t param_Rth;
static parameter_t param_Cth;


static float low_batt_th = LOW_BATT_TH;

static bool control_en = false;
static bool control_request_termination = false;
static bool control_running = false;


void control_enable(bool en)
{
    control_en = en;
    if (en) {
        motor_pwm_set(0);
        motor_pwm_enable();
    } else {
        motor_pwm_disable();
    }
}

void control_update_position_setpoint(float pos)
{
    float current_pos = ctrl.position;
    float current_vel = ctrl.velocity;
    chBSemWait(&setpoint_interpolation_lock);
    setpoint_update_position(&setpoint_interpolation, pos, current_pos, current_vel);
    chBSemSignal(&setpoint_interpolation_lock);
}

void control_update_velocity_setpoint(float vel)
{
    float current_vel = ctrl.velocity;
    chBSemWait(&setpoint_interpolation_lock);
    setpoint_update_velocity(&setpoint_interpolation, vel, current_vel);
    chBSemSignal(&setpoint_interpolation_lock);
}

void control_update_torque_setpoint(float torque)
{
    chBSemWait(&setpoint_interpolation_lock);
    setpoint_update_torque(&setpoint_interpolation, torque);
    chBSemSignal(&setpoint_interpolation_lock);
}

void control_update_trajectory_setpoint(float pos, float vel, float acc,
                                        float torque, timestamp_t ts)
{
    chBSemWait(&setpoint_interpolation_lock);
    setpoint_update_trajectory(&setpoint_interpolation, pos, vel, acc, torque, ts);
    chBSemSignal(&setpoint_interpolation_lock);
}


float control_get_motor_voltage(void)
{
    return ctrl.motor_voltage;
}

float control_get_vel_ctrl_out(void)
{
    return ctrl.velocity_ctrl_out;
}

float control_get_pos_ctrl_out(void)
{
    return ctrl.position_ctrl_out;
}

float control_get_current(void)
{
    return ctrl.current;
}

float control_get_torque(void)
{
    return ctrl.torque;
}

float control_get_velocity(void)
{
    return ctrl.velocity;
}

float control_get_position(void)
{
    return ctrl.position;
}

float control_get_current_error(void)
{
    return ctrl.current_error;
}

float control_get_velocity_error(void)
{
    return ctrl.velocity_error;
}

float control_get_position_error(void)
{
    return ctrl.position_error;
}

float control_get_current_setpoint(void)
{
    return ctrl.current_setpoint;
}

float control_get_velocity_setpoint(void)
{
    return ctrl.velocity_setpoint;
}

float control_get_position_setpoint(void)
{
    return ctrl.position_setpoint;
}


static void set_motor_voltage(float u)
{
    float u_batt = analog_get_battery_voltage();
    motor_pwm_set(u / u_batt);
}


static void pid_param_declare(struct pid_param_s *p, parameter_namespace_t *ns)
{
    parameter_scalar_declare_with_default(&p->kp, ns, "kp", 0);
    parameter_scalar_declare_with_default(&p->ki, ns, "ki", 0);
    parameter_scalar_declare_with_default(&p->kd, ns, "kd", 0);
    parameter_scalar_declare_with_default(&p->i_limit, ns, "i_limit", INFINITY);
}

static void pid_param_update(struct pid_param_s *p, pid_ctrl_t *ctrl)
{
    if (parameter_changed(&p->kp) ||
        parameter_changed(&p->ki) ||
        parameter_changed(&p->kd)) {
        pid_set_gains(ctrl, parameter_scalar_get(&p->kp),
                            parameter_scalar_get(&p->ki),
                            parameter_scalar_get(&p->kd));
        pid_reset_integral(ctrl);
    }
    if (parameter_changed(&p->i_limit)) {
        pid_set_integral_limit(ctrl, parameter_scalar_get(&p->i_limit));
    }
}


static void declare_parameters(void)
{
    parameter_namespace_declare(&param_ns_control, &parameter_root_ns, "control");
    parameter_scalar_declare_with_default(&param_low_batt_th, &param_ns_control, "low_batt_th", LOW_BATT_TH);
    parameter_scalar_declare(&param_vel_limit, &param_ns_control, "velocity_limit");
    parameter_scalar_declare(&param_torque_limit, &param_ns_control, "torque_limit");
    parameter_scalar_declare(&param_acc_limit, &param_ns_control, "acceleration_limit");

    parameter_namespace_declare(&param_ns_pos_ctrl, &param_ns_control, "position");
    pid_param_declare(&pos_pid_params, &param_ns_pos_ctrl);

    parameter_namespace_declare(&param_ns_vel_ctrl, &param_ns_control, "velocity");
    pid_param_declare(&vel_pid_params, &param_ns_vel_ctrl);

    parameter_namespace_declare(&param_ns_cur_ctrl, &param_ns_control, "current");
    pid_param_declare(&cur_pid_params, &param_ns_cur_ctrl);


    parameter_namespace_declare(&param_ns_motor, &parameter_root_ns, "motor");
    parameter_scalar_declare(&param_torque_cst, &param_ns_motor, "torque_cst");

    parameter_namespace_declare(&param_ns_thermal, &parameter_root_ns, "thermal");
    parameter_scalar_declare(&param_current_gain, &param_ns_thermal, "current_gain");
    parameter_scalar_declare(&param_max_temp, &param_ns_thermal, "max_temp");
    parameter_scalar_declare(&param_Rth, &param_ns_thermal, "Rth");
    parameter_scalar_declare(&param_Cth, &param_ns_thermal, "Cth");
}


void control_init(void)
{
    declare_parameters();

    ctrl.motor_current_constant = 1;
    ctrl.velocity_limit = 0;
    ctrl.torque_limit = 0;
    ctrl.current_limit = INFINITY;
    pid_init(&ctrl.current_pid);
    pid_init(&ctrl.velocity_pid);
    pid_init(&ctrl.position_pid);
    pid_set_frequency(&ctrl.current_pid, ANALOG_CONVERSION_FREQUENCY);
    pid_set_frequency(&ctrl.velocity_pid, ANALOG_CONVERSION_FREQUENCY);
    pid_set_frequency(&ctrl.position_pid, ANALOG_CONVERSION_FREQUENCY);

    setpoint_init(&setpoint_interpolation);
    chBSemObjectInit(&setpoint_interpolation_lock, false);

    control_feedback.output.position = 0;
    control_feedback.output.velocity = 0;
    control_feedback.primary_encoder.accumulator = 0;
    control_feedback.secondary_encoder.accumulator = 0;
}



static void update_parameters(void)
{
    if (parameter_namespace_contains_changed(&param_ns_control)) {
        if (parameter_namespace_contains_changed(&param_ns_pos_ctrl)) {
            pid_param_update(&pos_pid_params, &ctrl.position_pid);
        }
        if (parameter_namespace_contains_changed(&param_ns_vel_ctrl)) {
            pid_param_update(&vel_pid_params, &ctrl.velocity_pid);
        }
        if (parameter_namespace_contains_changed(&param_ns_cur_ctrl)) {
            pid_param_update(&cur_pid_params, &ctrl.current_pid);
        }
        if (parameter_changed(&param_low_batt_th)) {
            low_batt_th = parameter_scalar_get(&param_low_batt_th);
        }
        if (parameter_changed(&param_vel_limit)) {
            ctrl.velocity_limit = parameter_scalar_get(&param_vel_limit);
            chBSemWait(&setpoint_interpolation_lock);
            setpoint_set_velocity_limit(&setpoint_interpolation, ctrl.velocity_limit);
            chBSemSignal(&setpoint_interpolation_lock);
        }
        if (parameter_changed(&param_torque_limit)) {
            ctrl.torque_limit = parameter_scalar_get(&param_torque_limit);
        }
        if (parameter_changed(&param_acc_limit)) {
            chBSemWait(&setpoint_interpolation_lock);
            setpoint_set_acceleration_limit(&setpoint_interpolation, parameter_scalar_get(&param_acc_limit));
            chBSemSignal(&setpoint_interpolation_lock);
        }
    }
    if (parameter_namespace_contains_changed(&param_ns_motor)) {
        if (parameter_changed(&param_torque_cst)) {
            ctrl.motor_current_constant = parameter_scalar_get(&param_torque_cst);
        }
    }
    if (parameter_namespace_contains_changed(&param_ns_thermal)) {
        motor_protection_init(&control_motor_protection,
                              parameter_scalar_get(&param_max_temp),
                              parameter_scalar_get(&param_Rth),
                              parameter_scalar_get(&param_Cth),
                              parameter_scalar_get(&param_current_gain));
    }
}


#define CONTROL_WAKEUP_EVENT 1

static THD_FUNCTION(control_loop, arg)
{
    (void)arg;
    chRegSetThreadName("Control Loop");

    control_feedback.primary_encoder.previous = encoder_get_primary();
    control_feedback.secondary_encoder.previous = encoder_get_secondary();

    static event_listener_t analog_event_listener;
    chEvtRegisterMaskWithFlags(&analog_event, &analog_event_listener,
                               (eventmask_t)CONTROL_WAKEUP_EVENT,
                               (eventflags_t)ANALOG_EVENT_CONVERSION_DONE);


    const float delta_t = 1/(float)ANALOG_CONVERSION_FREQUENCY;
    while (!control_request_termination) {

        update_parameters();

        if (!control_en || analog_get_battery_voltage() < low_batt_th) {
            pid_reset_integral(&ctrl.current_pid);
            pid_reset_integral(&ctrl.velocity_pid);
            pid_reset_integral(&ctrl.position_pid);
            motor_protection_update(&control_motor_protection, analog_get_motor_current(), delta_t);
        } else {

            // sensor feedback
            control_feedback.input.potentiometer = analog_get_auxiliary();
            control_feedback.input.primary_encoder = encoder_get_primary();
            control_feedback.input.secondary_encoder = encoder_get_secondary();
            control_feedback.input.delta_t = delta_t;

            feedback_compute(&control_feedback);

            ctrl.periodic_actuator = control_feedback.output.actuator_is_periodic;
            ctrl.position = control_feedback.output.position;
            ctrl.velocity = ctrl.velocity * 0.9 + control_feedback.output.velocity * 0.1;
            ctrl.current = analog_get_motor_current();

            // ctrl.current_limit = motor_protection_update(&control_motor_protection, ctrl.current, delta_t);

            // setpoints
            chBSemWait(&setpoint_interpolation_lock);
            setpoint_compute(&setpoint_interpolation, &ctrl.setpts, delta_t);
            chBSemSignal(&setpoint_interpolation_lock);

            // run control step
            pid_cascade_control(&ctrl);

            set_motor_voltage(ctrl.motor_voltage);
        }

        chEvtWaitAny(CONTROL_WAKEUP_EVENT);
        chEvtGetAndClearFlags(&analog_event_listener);
    }

    set_motor_voltage(0);
    chEvtUnregister(&analog_event, &analog_event_listener);
    control_running = false;
    return 0;
}

void control_start(void)
{
    control_running = true;
    static THD_WORKING_AREA(control_loop_wa, 256);
    chThdCreateStatic(control_loop_wa, sizeof(control_loop_wa), HIGHPRIO, control_loop, NULL);
}

void control_stop(void)
{
    control_request_termination = true;
    while (control_running) {
        chThdSleepMilliseconds(1);
    }
    control_request_termination = false;
}
