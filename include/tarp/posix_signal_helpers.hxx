#pragma once

/*
 * C */
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

/*
 * Cxx */
#include <initializer_list>

/*
 * NOTE
 * Should be replaced with std::atomic_flag once we are ready to move
 * on to c++20. C++20 endows std::atomic_flag with a .test() method whereas
 * in earlier versions you have to do it backward and more awkwardly
 * (test_and_set) hence I've just stuck with the old sig_atomic_t for now.
 *
 * This is however only async-signal safe, not thread-safe and so
 * checking and setting the signal_handler_flag is subject to the usual
 * race conditions. The flag here is meant and assumed to be used in a scenario
 * where it is periodically checked such that in the worst case scenario
 * we miss a break by one loop pass, which is expected to be a non-issue.
 *
 * Otherwise if stricter measures are required, one possibility is blocking
 * the signals in all threads and having a single thread dedicated to signal
 * handling.
 */
static volatile sig_atomic_t signal_handler_flag = 0;

inline bool signal_caught( void )
{
	return signal_handler_flag == 1;
}

#define SET_SIGNAL_HANDLER( signal, sig_action_struct, exit_on_fail )                                                      \
	if( sigaction( signal, sig_action_struct, NULL ) == -1 )                                                           \
	{                                                                                                                  \
		fprintf( stderr, "Failed to set %s signal handler: '%s'", strsignal( signal ), strerror( errno ) );            \
		if( exit_on_fail )                                                                                             \
			exit( EXIT_FAILURE );                                                                                      \
	}

inline void simple_flag_setting_signal_handler( int )
{
	signal_handler_flag = 1;
}

/**
 * @brief Set a signal handler for specified signals.
 *
 * @details By default, this will set the simple_flag_setting_signal_handler
 * function as the signal handler for SIGINT, SIGTERM, SIGSEGV.
 * Both the handler and the list of signals can be overridden if need be.
 *
 * @param signals a list of signals to register the signal handler for
 *
 * @param sighandler the signal handler to register for the given signals
 *
 * @param exit_on_fail whether to exit the program if signal registration
 * fails for any signal
 *
 * @param verbose whether to print out a message for each signal as the handler
 * is registered.
 */
inline void set_up_signals( std::initializer_list< int > signals = { SIGINT, SIGTERM, SIGSEGV },
							sighandler_t sighandler = simple_flag_setting_signal_handler,
							bool exit_on_fail = true,
							bool verbose = true )
{
	struct sigaction sa;
	memset( &sa, 0, sizeof( sa ) );
	sa.sa_handler = sighandler;
	sigemptyset( &sa.sa_mask );

	for( auto& sig : signals )
	{
		if( verbose )
			fprintf( stderr, "setting up signal for: %s\n", strsignal( sig ) );
		SET_SIGNAL_HANDLER( sig, &sa, exit_on_fail );
	}
}

#undef SET_SIGNAL_HANDLER

