using UnityEngine;

namespace KumariKandam.Core
{
    /// <summary>
    /// A generic, thread-safe Singleton base class for MonoBehaviours.
    /// Ensures only one instance of the manager exists in the project.
    /// </summary>
    public abstract class Singleton<T> : MonoBehaviour where T : MonoBehaviour
    {
        private static T _instance;
        private static readonly object _lock = new object();
        private static bool _applicationIsQuitting = false;

        public static T Instance
        {
            get
            {
                if (_applicationIsQuitting)
                {
                    Debug.LogWarning($"[Singleton] Instance of '{typeof(T)}' already destroyed on application quit. Returning null.");
                    return null;
                }

                lock (_lock)
                {
                    if (_instance == null)
                    {
                        // FindFirstObjectByType is the standard and recommended way in Unity 6
                        _instance = (T)FindFirstObjectByType(typeof(T));

                        if (_instance == null)
                        {
                            GameObject singleton = new GameObject();
                            _instance = singleton.AddComponent<T>();
                            singleton.name = $"{typeof(T)} (Singleton)";

                            DontDestroyOnLoad(singleton);
                            Debug.Log($"[Singleton] An instance of {typeof(T)} was dynamically created with DontDestroyOnLoad.");
                        }
                    }

                    return _instance;
                }
            }
        }

        protected virtual void Awake()
        {
            if (_instance == null)
            {
                _instance = this as T;
                if (transform.parent == null)
                {
                    DontDestroyOnLoad(gameObject);
                }
            }
            else if (_instance != this)
            {
                Debug.LogWarning($"[Singleton] Duplicate instance of {typeof(T)} detected on {gameObject.name}. Destroying duplicate.");
                Destroy(gameObject);
            }
        }

        protected virtual void OnApplicationQuit()
        {
            _applicationIsQuitting = true;
        }

        protected virtual void OnDestroy()
        {
            if (_instance == this)
            {
                _instance = null;
            }
        }
    }
}
