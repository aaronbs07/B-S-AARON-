using UnityEngine;

namespace KumariKandam.Gameplay
{
    /// <summary>
    /// Implements a smooth third-person orbital camera.
    /// Rotates around the player target using mouse input and smooths movement.
    /// </summary>
    public class CameraController : MonoBehaviour
    {
        [Header("Target Reference")]
        [Tooltip("The player transform the camera will follow and orbit.")]
        [SerializeField] private Transform target;
        [Tooltip("Position offset relative to the player target.")]
        [SerializeField] private Vector3 offset = new Vector3(0.0f, 2.0f, -4.0f);

        [Header("Camera Physics")]
        [Tooltip("Stiffness of positional camera tracking. Higher value = snappier.")]
        [SerializeField] private float smoothSpeed = 15.0f;

        [Header("Orbital Sensitivity")]
        [Tooltip("Multiplier for mouse sensitivity.")]
        [SerializeField] private float mouseSensitivity = 150.0f;
        [Tooltip("Limit vertical camera pitch rotation downwards.")]
        [SerializeField] private float minPitch = -20.0f;
        [Tooltip("Limit vertical camera pitch rotation upwards.")]
        [SerializeField] private float maxPitch = 70.0f;

        private float yaw;
        private float pitch;

        private void Start()
        {
            if (target != null)
            {
                Vector3 euler = transform.eulerAngles;
                yaw = euler.y;
                pitch = euler.x;
            }
        }

        private void LateUpdate()
        {
            if (target == null) return;

            // Stop moving/rotating if the game is paused or inactive
            if (Core.GameManager.Instance != null && Core.GameManager.Instance.CurrentState != Core.GameState.Playing)
            {
                return;
            }

            GatherRotationInput();
            ApplyCameraMovement();
        }

        private void GatherRotationInput()
        {
            yaw += Input.GetAxis("Mouse X") * mouseSensitivity * Time.deltaTime;
            pitch -= Input.GetAxis("Mouse Y") * mouseSensitivity * Time.deltaTime;
            pitch = Mathf.Clamp(pitch, minPitch, maxPitch);

            // Orient the target player along the camera's horizontal yaw axis
            target.rotation = Quaternion.Euler(0f, yaw, 0f);
        }

        private void ApplyCameraMovement()
        {
            // Determine camera rotation
            Quaternion targetRotation = Quaternion.Euler(pitch, yaw, 0f);

            // Compute camera position relative to target character
            Vector3 targetPosition = target.position + (targetRotation * offset);

            // Move and rotate camera
            transform.position = Vector3.Lerp(transform.position, targetPosition, smoothSpeed * Time.deltaTime);
            transform.rotation = targetRotation;
        }
    }
}
