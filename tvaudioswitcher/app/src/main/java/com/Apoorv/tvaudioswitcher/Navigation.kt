package com.Apoorv.tvaudioswitcher

import androidx.compose.runtime.Composable
//import androidx.navigation.NavHostController
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController

sealed class Screen(val route: String) {
    data object TvList : Screen("tv_list")
    data object Connect : Screen("connect/{tvIp}") {  // pass IP as argument
        fun createRoute(ip: String) = "connect/$ip"
    }
    data object AudioControl : Screen("audio_control/{tvIp}") {
        fun createRoute(ip: String) = "audio_control/$ip"
    }
}

@Composable
fun AppNavigation() {
    val navController = rememberNavController()

    NavHost(navController = navController, startDestination = Screen.TvList.route) {
        composable(Screen.TvList.route) {
            TvListScreen(onTvSelected = { tv ->
                navController.navigate(Screen.Connect.createRoute(tv.ip))
            })
        }

        composable(Screen.Connect.route) { backStackEntry ->
            val ip = backStackEntry.arguments?.getString("tvIp") ?: ""
            ConnectScreen(ip = ip, onConnected = {
                navController.navigate(Screen.AudioControl.createRoute(ip))
            })
        }

        composable(Screen.AudioControl.route) { backStackEntry ->
            val ip = backStackEntry.arguments?.getString("tvIp") ?: ""
            AudioControlScreen(ip = ip)
        }
    }
}