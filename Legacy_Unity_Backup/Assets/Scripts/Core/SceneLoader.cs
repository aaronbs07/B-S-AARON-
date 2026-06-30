using System;
using System.Collections;
using UnityEngine;
using UnityEngine.SceneManagement;

namespace KumariKandam.Core
{
    /// <summary>
    /// Handles asynchronous scene loading with progress reporting.
    /// </summary>
    public class SceneLoader : Singleton<SceneLoader>
    {
        public float LoadingProgress { get; private set; }
        public bool IsLoading { get; private set; }

        // Broadcast event for UI to display progress bar
        public static event Action<float> OnLoadingProgressChanged;

        /// <summary>
        /// Initiates asynchronous scene loading.
        /// </summary>
        /// <param name="sceneName">Name of the scene file to load.</param>
        /// <param name="onComplete">Optional callback executed when load finishes.</param>
        public void LoadSceneAsync(string sceneName, Action onComplete = null)
        {
            if (IsLoading)
            {
                Debug.LogWarning($"[SceneLoader] Already loading. Load request for '{sceneName}' was rejected.");
                return;
            }

            StartCoroutine(LoadSceneRoutine(sceneName, onComplete));
        }

        private IEnumerator LoadSceneRoutine(string sceneName, Action onComplete)
        {
            IsLoading = true;
            LoadingProgress = 0f;
            OnLoadingProgressChanged?.Invoke(0f);

            AsyncOperation operation = SceneManager.LoadSceneAsync(sceneName);
            if (operation == null)
            {
                Debug.LogError($"[SceneLoader] Unable to load scene '{sceneName}'. Ensure it is added to the Build Settings.");
                IsLoading = false;
                yield break;
            }

            // Keep loading until done
            while (!operation.isDone)
            {
                // Unity scene load operations report progress up to 0.9. Normalize this to 0-1 range.
                LoadingProgress = Mathf.Clamp01(operation.progress / 0.9f);
                OnLoadingProgressChanged?.Invoke(LoadingProgress);
                yield return null;
            }

            LoadingProgress = 1.0f;
            OnLoadingProgressChanged?.Invoke(1.0f);
            IsLoading = false;

            onComplete?.Invoke();
        }
    }
}
