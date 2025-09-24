# PaintSplash Map Format Specification
## Complete Component Property Reference for YAML Map Files

This document outlines all component properties that should be supported in YAML map files for comprehensive level editing and loading capabilities.

## What Belongs in Map Files

Map files contain **level design data** - the static, authored content that defines a playable level. This includes:

- World geometry (brushes, faces, materials)
- Entity placement and properties
- Lighting setup
- Audio source positioning and settings
- Physics colliders and rigidbodies
- Spawn points and waypoints

## What Does NOT Belong in Map Files

Map files should NOT contain **transient state** or **user preferences**:

- Master volume levels
- Mute states
- Global audio processing settings
- User interface preferences
- Runtime state that changes during gameplay
- Save game data

These settings are stored in application preferences, save files, or managed by individual systems at runtime.

## Table of Contents
1. [World Geometry](#world-geometry)
2. [Transform Components](#transform-components)
3. [Material Components](#material-components)
4. [Light Components](#light-components)
5. [Mesh/Model Components](#meshmodel-components)
6. [Sprite Components](#sprite-components)
7. [Audio Components](#audio-components)
8. [Physics Components](#physics-components)
9. [Gameplay/Script Components](#gameplayscript-components)
10. [Global Map Properties](#global-map-properties)

---

## World Geometry

### Brush Structure
```yaml
world:
  brushes:
  - id: 1
    faces:
    - vertices:
      - [x1, y1, z1]
      - [x2, y2, z2]
      - [x3, y3, z3]
      - [x4, y4, z4]
      uvs:
      - [u1, v1]
      - [u2, v2]
      - [u3, v3]
      - [u4, v4]
      material: material_id
      tint: [r, g, b, a]
      render_mode: "default|vertex_colors|wireframe|invisible"
```

### Face Properties
- **vertices**: Array of 3D vertex positions `[x, y, z]`
- **uvs**: Array of 2D texture coordinates `[u, v]`
- **material**: Reference to material ID from materials section
- **tint**: Color tint multiplier `[r, g, b, a]` (0-255)
- **render_mode**: Rendering mode override

---

## Transform Components

### Standard Transform
```yaml
transform:
  position: [x, y, z]
  rotation: [x, y, z, w]  # Quaternion
  scale: [x, y, z]
  parent: entity_id       # Optional parent entity ID
```

### Alternative Rotation Formats
```yaml
transform:
  position: [x, y, z]
  rotation_euler: [pitch, yaw, roll]  # Degrees
  rotation_axis_angle: [[x, y, z], angle]  # Axis + degrees
  scale: [x, y, z]
```

---

## Material Components

### Basic Material Properties
```yaml
material:
  # Basic shading properties
  diffuseColor: [r, g, b, a]      # Base color (0-255)
  specularColor: [r, g, b, a]     # Specular highlight color
  shininess: 32.0                  # Specular exponent (0-128)
  alpha: 1.0                       # Opacity (0.0-1.0)

  # PBR properties
  roughness: 0.5                   # Surface roughness (0.0-1.0)
  metallic: 0.0                    # Metalness (0.0-1.0)
  ao: 1.0                          # Ambient occlusion (0.0-1.0)

  # Emission properties
  emissiveColor: [r, g, b, a]      # Emissive color
  emissiveIntensity: 1.0           # Emission multiplier

  # Material type
  type: "BASIC"                    # BASIC|PBR|UNLIT|EMISSIVE|TRANSPARENT

  # Texture maps (relative to assets/)
  diffuseMap: "textures/wall_diffuse.png"
  normalMap: "textures/wall_normal.png"
  specularMap: "textures/wall_specular.png"
  roughnessMap: "textures/wall_roughness.png"
  metallicMap: "textures/wall_metallic.png"
  aoMap: "textures/wall_ao.png"
  emissiveMap: "textures/wall_emissive.png"

  # Rendering flags
  doubleSided: false               # Render both face sides
  depthWrite: true                 # Write to depth buffer
  depthTest: true                  # Enable depth testing
  castShadows: true                # Cast shadows

  # Shader parameters (for custom shaders)
  shaderParams:
    params: [0.0, 0.0, 0.0, 0.0]
    vectorParams: [[0,0,0,0], [0,0,0,0]]
    colorParams: [[255,255,255,255], [255,255,255,255]]
```

---

## Light Components

### Point Light
```yaml
light:
  type: "point"
  color: [r, g, b]                 # Light color (0-255)
  intensity: 1.0                   # Light intensity multiplier
  range: 10.0                      # Maximum light distance
  attenuation:
    constant: 1.0                  # Constant attenuation
    linear: 0.1                    # Linear attenuation
    quadratic: 0.01                # Quadratic attenuation

  # Shadow properties
  castShadows: true                # Enable shadow casting
  shadowMapSize: 1024              # Shadow map resolution
  shadowBias: 0.005               # Shadow bias to prevent acne
```

### Directional Light
```yaml
light:
  type: "directional"
  color: [r, g, b]
  intensity: 1.0
  direction: [x, y, z]             # Light direction vector

  # Shadow properties
  castShadows: true
  shadowMapSize: 2048
  shadowBias: 0.001
  shadowDistance: 100.0            # Maximum shadow distance
```

### Spot Light
```yaml
light:
  type: "spot"
  color: [r, g, b]
  intensity: 1.0
  range: 20.0
  direction: [x, y, z]             # Spot direction
  innerAngle: 30.0                 # Inner cone angle (degrees)
  outerAngle: 45.0                 # Outer cone angle (degrees)
  attenuation:
    constant: 1.0
    linear: 0.1
    quadratic: 0.01

  # Shadow properties
  castShadows: true
  shadowMapSize: 1024
  shadowBias: 0.005
```

---

## Mesh/Model Components

### Static Model
```yaml
mesh:
  type: "model"
  model: "models/wall.obj"         # Model file path
  material: material_id            # Override material ID
  castShadows: true                # Cast shadows
  receiveShadows: true             # Receive shadows
```

### Primitive Shapes
```yaml
mesh:
  type: "primitive"
  shape: "cube|sphere|cylinder|cone|plane"
  size: [width, height, depth]     # Dimensions
  subdivisions: 8                  # For spheres/cylinders
  material: material_id
  castShadows: true
  receiveShadows: true
```

### Complex Composite Mesh
```yaml
mesh:
  type: "composite"
  parts:
  - shape: "cube"
    size: [1, 1, 1]
    position: [0, 0, 0]
    rotation: [0, 0, 0, 1]
    material: material_id
  - shape: "cylinder"
    size: [0.5, 2, 0.5]
    position: [1, 0, 0]
    material: material_id
  castShadows: true
  receiveShadows: true
```

---

## Sprite Components

### 2D Sprite
```yaml
sprite:
  texture: "textures/sprite.png"
  size: [width, height]            # Sprite dimensions
  pivot: [x, y]                    # Pivot point (0-1)
  pixelsPerUnit: 100               # Pixels to world units ratio

  # Animation properties
  animated: false
  animation:
    frames:
    - "textures/sprite_frame1.png"
    - "textures/sprite_frame2.png"
    framesPerSecond: 12
    loop: true

  # Rendering properties
  color: [r, g, b, a]              # Tint color
  flipX: false                     # Horizontal flip
  flipY: false                     # Vertical flip
  sortingOrder: 0                  # Render order
  sortingLayer: "Default"          # Render layer
```

### Billboard Sprite
```yaml
sprite:
  texture: "textures/particle.png"
  size: [width, height]
  billboard: true                  # Always face camera
  color: [r, g, b, a]
  sortingOrder: 0
```

---

## Audio Components

### 3D Audio Source
```yaml
audio:
  # Audio category (determines default spatial behavior)
  audioType: "SFX_3D"              # SFX_3D|SFX_2D|MUSIC|UI|AMBIENT|VOICE|MASTER

  clip: "audio/ambient_wind.wav"   # Audio file path (relative to assets/)
  volume: 1.0                      # Volume multiplier (0.0-1.0)
  pitch: 1.0                       # Pitch multiplier (0.5-2.0)
  loop: false                      # Loop playback
  playOnStart: false               # Auto-play when entity is created

  # 3D spatial audio properties (ignored for 2D audio types)
  spatialBlend: 0.0                # 2D=0.0, 3D=1.0 (auto-set based on audioType)
  minDistance: 1.0                 # Minimum attenuation distance
  maxDistance: 50.0                # Maximum attenuation distance
  rolloffMode: "Linear"            # Linear|Logarithmic|Custom
  dopplerLevel: 1.0                # Doppler effect intensity (0.0-5.0)
  spread: 0.0                      # Stereo spread angle (0-360 degrees)
  reverbZoneMix: 1.0               # Reverb zone mix (0.0-1.1)

  # Playback properties
  priority: 128                    # Playback priority (0-256, lower = higher priority)

  # Note: Runtime audio processing flags (mute, bypass effects, etc.)
  # are managed by the AudioSystem and stored in application state,
  # not in map files. Only design-time properties are stored here.

  # Output routing (design-time only)
  outputAudioMixerGroup: "Master"  # Audio mixer group for routing

  # Audio metadata
  audioName: "ambient_wind"        # Display name for the audio source
```

#### Audio Type Categories

| AudioType | Spatial | Use Case | Default spatialBlend |
|-----------|---------|----------|---------------------|
| `SFX_3D` | 3D | Positional sound effects | 1.0 |
| `SFX_2D` | 2D | Non-positional SFX | 0.0 |
| `MUSIC` | 2D | Background music | 0.0 |
| `UI` | 2D | Interface sounds | 0.0 |
| `AMBIENT` | 3D | Environmental audio | 0.8 |
| `VOICE` | 2D/3D | Dialogue/VO | 0.0-1.0 |
| `MASTER` | 2D | System audio | 0.0 |

### Audio Listener
```yaml
audio_listener:
  active: true                      # Is this the active listener?
  # Note: Master volume, mute states, and global audio settings
  # are stored in application preferences, not in map files
```

---

## Physics Components

### Rigid Body
```yaml
rigidbody:
  type: "dynamic"                   # dynamic|kinematic|static
  mass: 1.0                         # Object mass (kg)
  drag: 0.1                         # Linear drag coefficient
  angularDrag: 0.05                 # Angular drag coefficient
  useGravity: true                  # Affected by gravity
  isKinematic: false                # Manually controlled movement

  # Constraints
  freezePosition: [false, false, false]  # Freeze X,Y,Z position
  freezeRotation: [false, false, false]  # Freeze X,Y,Z rotation

  # Collision detection
  collisionDetection: "discrete"    # discrete|continuous|continuous_dynamic
```

### Collider Shapes
```yaml
collider:
  type: "box"                       # box|sphere|capsule|mesh
  size: [width, height, depth]      # For box collider
  radius: 1.0                       # For sphere/capsule
  height: 2.0                       # For capsule
  mesh: "models/collision.obj"      # For mesh collider

  # Physics material
  material:
    friction: 0.6                   # Surface friction
    bounciness: 0.0                 # Restitution/bounce
    frictionCombine: "average"      # average|minimum|maximum|multiply
    bounceCombine: "average"

  # Trigger properties
  isTrigger: false                  # Is this a trigger volume?
  triggerEvents: true               # Generate trigger events
```

---

## Gameplay/Script Components

### Script Component
```yaml
script:
  class: "RotateScript"             # Script class name
  enabled: true                     # Script enabled state

  # Script parameters (passed to script constructor/init)
  parameters:
    speed: 90.0                     # Rotation speed (degrees/sec)
    axis: [0, 1, 0]                 # Rotation axis
    range: [-45, 45]                # Rotation limits

  # Event handlers
  onStart: "InitRotation"
  onUpdate: "UpdateRotation"
  onCollisionEnter: "OnHit"
  onTriggerEnter: "OnPlayerEnter"
```

### Health Component
```yaml
health:
  maxHealth: 100
  currentHealth: 100
  invincible: false
  invincibleTime: 0.0               # Seconds of invincibility after damage

  # Damage properties
  damageMultiplier: 1.0             # Global damage multiplier
  damageTypeMultipliers:           # Per damage type
    bullet: 1.0
    fire: 1.5
    poison: 0.5

  # Death properties
  destroyOnDeath: true
  deathEffect: "particles/explosion.particle"
  respawnTime: 5.0
  respawnPosition: [x, y, z]
```

### Player Controller
```yaml
player_controller:
  moveSpeed: 5.0                    # Movement speed
  jumpForce: 10.0                   # Jump force
  airControl: 0.8                   # Air movement control (0-1)

  # Camera properties
  cameraOffset: [0, 1.8, -3]        # Camera position relative to player
  cameraSmoothing: 5.0              # Camera follow smoothing

  # Input settings
  mouseSensitivity: 2.0
  invertY: false
  controllerEnabled: true

  # Physics settings
  groundCheckDistance: 0.1
  slopeLimit: 45.0                  # Maximum climbable slope (degrees)
```

---

## Global Map Properties

### Map Header
```yaml
version: "2.1"
name: "My Awesome Level"
description: "A detailed level description"
author: "Level Designer Name"
created: "2025-01-21T12:00:00Z"
modified: "2025-01-21T14:30:00Z"
gameVersion: "1.0.0"
```

### Settings Section
```yaml
settings:
  # Physics
  gravity: [0.0, -980.0, 0.0]
  defaultFriction: 0.6
  defaultBounce: 0.0

  # Rendering
  fog:
    enabled: true
    color: [135, 206, 235, 255]
    start: 1000.0
    end: 5000.0
    density: 0.001

  # Sky/Environment
  skybox: "skybox/cubemap_cloudy.png"
  ambientLight: [64, 64, 64, 255]  # Ambient lighting color
  sunLight:
    direction: [0.5, -1.0, 0.3]
    color: [255, 255, 224, 255]
    intensity: 1.0

  # Audio
  masterVolume: 1.0
  musicVolume: 0.8
  sfxVolume: 1.0
  ambientVolume: 0.6

  # Gameplay
  timeLimit: 600.0                  # Level time limit (seconds)
  scoreTarget: 10000                # Target score
  difficulty: "normal"              # easy|normal|hard|expert
```

### Navigation Section
```yaml
navigation:
  navmesh:
    cellSize: 0.5                   # Navigation grid resolution
    cellHeight: 0.1                 # Vertical resolution
    agentHeight: 2.0                # Character height
    agentRadius: 0.5                # Character radius
    agentMaxClimb: 0.5              # Maximum climb height
    agentMaxSlope: 45.0             # Maximum slope angle

  # Pre-calculated navigation data
  waypoints:
  - id: 1
    position: [10, 0, 10]
    connections: [2, 3, 4]
  - id: 2
    position: [20, 0, 10]
    connections: [1, 5]

  # Cover points for AI
  cover_points:
  - position: [5, 0, 5]
    direction: [1, 0, 0]
    cover_type: "low"               # low|high|full
  - position: [15, 0, 15]
    direction: [0, 0, 1]
    cover_type: "high"
```

---

## Implementation Priority

### Phase 1 (Current - Basic Geometry)
- ✅ World geometry with UVs
- ✅ Basic materials (diffuse textures)
- ✅ Transform components
- ✅ Basic lighting

### Phase 2 (Advanced Materials)
- 🔄 Full MaterialComponent properties
- 🔄 PBR material support
- 🔄 Normal/specular/roughness maps
- 🔄 Emissive materials

### Phase 3 (Advanced Components)
- ⏳ Complete LightComponent properties
- ⏳ Mesh/Model loading
- ⏳ Sprite rendering
- ⏳ Audio components

### Phase 4 (Gameplay Systems)
- ⏳ Physics components
- ⏳ Script components
- ⏳ AI/Navigation components
- ⏳ Particle systems

This specification provides a comprehensive roadmap for extending the YAML parser to support all game components that an editor would need to create and modify.
