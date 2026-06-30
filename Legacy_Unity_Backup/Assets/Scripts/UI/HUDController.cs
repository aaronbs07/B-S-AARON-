using UnityEngine;
using UnityEngine.UI;
using KumariKandam.Core;

namespace KumariKandam.UI
{
    /// <summary>
    /// Manages UI overlays visible during gameplay (HUD, Pause, Game Over, Loading screens).
    /// </summary>
    public class HUDController : MonoBehaviour
    {
        [Header("UI Panels")]
        [Tooltip("Active gameplay HUD (health, crosshair, score, etc).")]
        [SerializeField] private GameObject hudPanel;
        [Tooltip("Pause overlay screen.")]
        [SerializeField] private GameObject pausePanel;
        [Tooltip("Game Over overlay screen.")]
        [SerializeField] private GameObject gameOverPanel;
        [Tooltip("Asynchronous scene transition loading screen.")]
        [SerializeField] private GameObject loadingPanel;

        [Header("UI Components")]
        [Tooltip("Progress bar for visualising scene load levels.")]
        [SerializeField] private Slider loadingBar;

        [Header("Buttons")]
        [SerializeField] private Button resumeButton;
        [SerializeField] private Button restartButton;
        [SerializeField] private Button menuButton;

        private void OnEnable()
        {
            // Hook game framework events
            GameManager.OnStateChanged += HandleStateChanged;
            SceneLoader.OnLoadingProgressChanged += HandleLoadingProgress;
        }

        private void OnDisable()
        {
            // Unhook event listeners to prevent memory leaks
            GameManager.OnStateChanged -= HandleStateChanged;
            SceneLoader.OnLoadingProgressChanged -= HandleLoadingProgress;
        }

        private void Start()
        {
            // Attach UI button callbacks
            if (resumeButton != null) resumeButton.onClick.AddListener(OnResumeClicked);
            if (restartButton != null) restartButton.onClick.AddListener(OnRestartClicked);
            if (menuButton != null) menuButton.onClick.AddListener(OnMenuClicked);

            // Fetch current state of game and render UI appropriately
            if (GameManager.Instance != null)
            {
                HandleStateChanged(GameManager.Instance.CurrentState);
            }
        }

        private void Update()
        {
            // Check for Escape keys to trigger game pause state
            if (Input.GetKeyDown(KeyCode.Escape))
            {
                if (GameManager.Instance != null)
                {
                    GameManager.Instance.TogglePause();
                }
            }
        }

        private void HandleStateChanged(GameState state)
        {
            // Toggle panel active states in accordance with game status
            if (hudPanel != null) hudPanel.SetActive(state == GameState.Playing || state == GameState.Paused);
            if (pausePanel != null) pausePanel.SetActive(state == GameState.Paused);
            if (gameOverPanel != null) gameOverPanel.SetActive(state == GameState.GameOver);
            if (loadingPanel != null) loadingPanel.SetActive(state == GameState.Loading);
        }

        private void HandleLoadingProgress(float progress)
        {
            if (loadingBar != null)
            {
                loadingBar.value = progress;
            }
        }

        private void OnResumeClicked()
        {
            if (GameManager.Instance != null)
            {
                GameManager.Instance.TogglePause();
            }
        }

        private void OnRestartClicked()
        {
            if (GameManager.Instance != null)
            {
                GameManager.Instance.StartGame();
            }
        }

        private void OnMenuClicked()
        {
            if (GameManager.Instance != null)
            {
                GameManager.Instance.ReturnToMainMenu();
            }
        }
    }
}
