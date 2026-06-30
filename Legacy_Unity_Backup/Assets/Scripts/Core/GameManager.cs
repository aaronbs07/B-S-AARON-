using System;
using UnityEngine;

namespace KumariKandam.Core
{
    /// <summary>
    /// Supported game states in Kumari Kandam.
    /// </summary>
    public enum GameState
    {
        MainMenu,
        Loading,
        Playing,
        Paused,
        GameOver
    }

    /// <summary>
    /// Central manager responsible for game flow, state machine, and pausing.
    /// </summary>
    public class GameManager : Singleton<GameManager>
    {
        public GameState CurrentState { get; private set; } = GameState.MainMenu;

        // Broadcast events when game states toggle
        public static event Action<GameState> OnStateChanged;
        public static event Action<bool> OnPauseToggled;

        protected override void Awake()
        {
            base.Awake();
            // Perform application-wide initializations (e.g. framerate caps)
            Application.targetFrameRate = 60;
        }

        /// <summary>
        /// Transitions the game to a new state and adjusts time scales.
        /// </summary>
        public void ChangeState(GameState newState)
        {
            if (CurrentState == newState) return;

            CurrentState = newState;
            Debug.Log($"[GameManager] Transitioning to GameState: {newState}");

            // Apply state specific rules (like freezing physics/time during pause)
            switch (newState)
            {
                case GameState.MainMenu:
                    Time.timeScale = 1.0f;
                    break;
                case GameState.Loading:
                    // Time scale remains active to process loading screen animations if necessary
                    Time.timeScale = 1.0f;
                    break;
                case GameState.Playing:
                    Time.timeScale = 1.0f;
                    Cursor.lockState = CursorLockMode.Locked;
                    Cursor.visible = false;
                    break;
                case GameState.Paused:
                    Time.timeScale = 0.0f;
                    Cursor.lockState = CursorLockMode.None;
                    Cursor.visible = true;
                    break;
                case GameState.GameOver:
                    Time.timeScale = 0.0f;
                    Cursor.lockState = CursorLockMode.None;
                    Cursor.visible = true;
                    break;
            }

            OnStateChanged?.Invoke(newState);
        }

        /// <summary>
        /// Toggles between Paused and Playing game states.
        /// </summary>
        public void TogglePause()
        {
            if (CurrentState == GameState.Playing)
            {
                ChangeState(GameState.Paused);
                OnPauseToggled?.Invoke(true);
            }
            else if (CurrentState == GameState.Paused)
            {
                ChangeState(GameState.Playing);
                OnPauseToggled?.Invoke(false);
            }
        }

        /// <summary>
        /// Convenience method to start the gameplay scene.
        /// </summary>
        public void StartGame()
        {
            ChangeState(GameState.Loading);
            SceneLoader.Instance.LoadSceneAsync("Game", () =>
            {
                ChangeState(GameState.Playing);
            });
        }

        /// <summary>
        /// Convenience method to return to the Main Menu.
        /// </summary>
        public void ReturnToMainMenu()
        {
            ChangeState(GameState.Loading);
            SceneLoader.Instance.LoadSceneAsync("MainMenu", () =>
            {
                ChangeState(GameState.MainMenu);
            });
        }

        /// <summary>
        /// Closes the application or exits Play Mode in editor.
        /// </summary>
        public void QuitGame()
        {
            Debug.Log("[GameManager] Shutting down application...");
#if UNITY_EDITOR
            UnityEditor.EditorApplication.isPlaying = false;
#else
            Application.Quit();
#endif
        }
    }
}
