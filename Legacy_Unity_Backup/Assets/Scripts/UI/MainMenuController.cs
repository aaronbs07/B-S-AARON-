using UnityEngine;
using UnityEngine.UI;
using KumariKandam.Core;

namespace KumariKandam.UI
{
    /// <summary>
    /// Handles Main Menu UI interactions and links user actions to the GameManager.
    /// </summary>
    public class MainMenuController : MonoBehaviour
    {
        [Header("UI Panels")]
        [Tooltip("The main container representing standard main menu buttons.")]
        [SerializeField] private GameObject mainMenuPanel;
        [Tooltip("Settings overlay containing settings toggles.")]
        [SerializeField] private GameObject settingsPanel;

        [Header("Buttons")]
        [SerializeField] private Button playButton;
        [SerializeField] private Button settingsButton;
        [SerializeField] private Button quitButton;
        [SerializeField] private Button backSettingsButton;

        private void Start()
        {
            // Set up button event listeners
            if (playButton != null) playButton.onClick.AddListener(OnPlayClicked);
            if (settingsButton != null) settingsButton.onClick.AddListener(OnSettingsClicked);
            if (quitButton != null) quitButton.onClick.AddListener(OnQuitClicked);
            if (backSettingsButton != null) backSettingsButton.onClick.AddListener(OnBackFromSettingsClicked);

            // Establish starting UI visibility
            ShowMainMenu();
        }

        private void OnPlayClicked()
        {
            Debug.Log("[MainMenuController] Play Game button clicked.");
            if (GameManager.Instance != null)
            {
                GameManager.Instance.StartGame();
            }
        }

        private void OnSettingsClicked()
        {
            Debug.Log("[MainMenuController] Settings button clicked.");
            if (mainMenuPanel != null) mainMenuPanel.SetActive(false);
            if (settingsPanel != null) settingsPanel.SetActive(true);
        }

        private void OnBackFromSettingsClicked()
        {
            Debug.Log("[MainMenuController] Back from settings button clicked.");
            ShowMainMenu();
        }

        private void OnQuitClicked()
        {
            Debug.Log("[MainMenuController] Quit button clicked.");
            if (GameManager.Instance != null)
            {
                GameManager.Instance.QuitGame();
            }
        }

        private void ShowMainMenu()
        {
            if (mainMenuPanel != null) mainMenuPanel.SetActive(true);
            if (settingsPanel != null) settingsPanel.SetActive(false);
        }
    }
}
