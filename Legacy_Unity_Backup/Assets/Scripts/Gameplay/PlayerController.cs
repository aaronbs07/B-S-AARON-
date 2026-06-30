using UnityEngine;

namespace KumariKandam.Gameplay
{
    /// <summary>
    /// Implements standard 3D player movement, gravity, and jumping using CharacterController.
    /// Supports Unity standard input axes out of the box.
    /// </summary>
    [RequireComponent(typeof(CharacterController))]
    public class PlayerController : MonoBehaviour
    {
        [Header("Movement Settings")]
        [Tooltip("Standard running speed.")]
        [SerializeField] private float moveSpeed = 6.0f;
        [Tooltip("Strength of gravitational pull.")]
        [SerializeField] private float gravity = -19.62f;
        [Tooltip("Height achieved at apex of jump.")]
        [SerializeField] private float jumpHeight = 2.0f;

        [Header("Grounding")]
        [Tooltip("Optional transform placed at character feet for precision ground checking.")]
        [SerializeField] private Transform groundCheck;
        [Tooltip("Radius around the ground check transform to detect floor layers.")]
        [SerializeField] private float groundDistance = 0.3f;
        [Tooltip("Layer mask representing walkable surfaces.")]
        [SerializeField] private LayerMask groundMask;

        private CharacterController controller;
        private Vector3 velocity;
        private bool isGrounded;

        private void Start()
        {
            controller = GetComponent<CharacterController>();
        }

        private void Update()
        {
            // Only update movement if the game state is currently Playing
            if (Core.GameManager.Instance != null && Core.GameManager.Instance.CurrentState != Core.GameState.Playing)
            {
                return;
            }

            ProcessGroundedState();
            ProcessMovement();
            ProcessJumpAndGravity();
        }

        private void ProcessGroundedState()
        {
            if (groundCheck != null)
            {
                isGrounded = Physics.CheckSphere(groundCheck.position, groundDistance, groundMask);
            }
            else
            {
                isGrounded = controller.isGrounded;
            }

            // Snaps velocity to small negative when grounded to avoid accumulating fall speed
            if (isGrounded && velocity.y < 0)
            {
                velocity.y = -2f;
            }
        }

        private void ProcessMovement()
        {
            float horizontal = Input.GetAxis("Horizontal");
            float vertical = Input.GetAxis("Vertical");

            // Compute direction vector relative to local forward and right
            Vector3 direction = transform.right * horizontal + transform.forward * vertical;

            // Apply movement
            controller.Move(direction * moveSpeed * Time.deltaTime);
        }

        private void ProcessJumpAndGravity()
        {
            // Jump trigger
            if (Input.GetButtonDown("Jump") && isGrounded)
            {
                velocity.y = Mathf.Sqrt(jumpHeight * -2.0f * gravity);
            }

            // Apply gravity over time
            velocity.y += gravity * Time.deltaTime;

            // Move the player character based on gravity changes
            controller.Move(velocity * Time.deltaTime);
        }

        // Draw ground check gizmo in Editor
        private void OnDrawGizmosSelected()
        {
            if (groundCheck != null)
            {
                Gizmos.color = Color.green;
                Gizmos.DrawWireSphere(groundCheck.position, groundDistance);
            }
        }
    }
}
