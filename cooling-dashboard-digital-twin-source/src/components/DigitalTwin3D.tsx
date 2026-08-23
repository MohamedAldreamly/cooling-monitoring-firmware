import {
  Html,
  OrbitControls,
} from '@react-three/drei'
import {
  Canvas,
  useFrame,
} from '@react-three/fiber'
import {
  Suspense,
  useMemo,
  useRef,
  useState,
} from 'react'
import type {
  Group,
  Mesh,
} from 'three'
import {
  CatmullRomCurve3,
  Vector3,
} from 'three'

import type {
  Alarm,
  TelemetrySignal,
} from '../types/api'


type MachineState =
  | 'normal'
  | 'warning'
  | 'alarm'
  | 'offline'


interface DigitalTwin3DProps {
  online: boolean
  signals: TelemetrySignal[]
  alarms: Alarm[]
}


const stateColors: Record<MachineState, string> = {
  normal: '#0ea5e9',
  warning: '#f59e0b',
  alarm: '#ef4444',
  offline: '#94a3b8',
}


function findSignal(
  signals: TelemetrySignal[],
  terms: string[],
) {
  return signals.find((signal) => {
    const name = signal.name?.toUpperCase() ?? ''
    return terms.some((term) => name.includes(term))
  })
}


function findExactSignal(
  signals: TelemetrySignal[],
  name: string,
) {
  return signals.find(
    (signal) => signal.name?.toUpperCase() === name,
  )
}


function temperatureColor(signal?: TelemetrySignal) {
  const value = numericValue(signal)

  if (value === undefined) return '#94a3b8'
  if (value <= -18) return '#38bdf8'
  if (value <= -12) return '#22c55e'
  if (value <= -8) return '#f59e0b'
  return '#ef4444'
}


function machineState(
  signal: TelemetrySignal | undefined,
  alarms: Alarm[],
  online: boolean,
): MachineState {
  if (!online) return 'offline'

  const alarm = alarms.find(
    (item) =>
      item.source_signal === signal?.name,
  )

  const severity = alarm?.severity?.toUpperCase()

  if (severity === 'CRITICAL' || severity === 'HIGH') {
    return 'alarm'
  }

  const quality = signal?.quality?.toLowerCase()

  if (alarm || quality === 'stale' || quality === 'uncertain') {
    return 'warning'
  }

  if (
    !signal
    || quality === 'fault'
    || quality === 'unavailable'
    || signal.value === null
    || signal.value === undefined
  ) {
    return 'offline'
  }

  return 'normal'
}


function signalValue(signal?: TelemetrySignal) {
  if (signal?.value === null || signal?.value === undefined) {
    return 'Unavailable'
  }

  const value =
    typeof signal.value === 'number'
      ? Number.isInteger(signal.value)
        ? String(signal.value)
        : signal.value.toFixed(1)
      : String(signal.value)

  return `${value}${signal.unit ? ` ${signal.unit}` : ''}`
}


function isRunning(signal?: TelemetrySignal) {
  return signal?.value === true
    || signal?.value === 1
    || String(signal?.value).toLowerCase() === 'running'
    || String(signal?.value).toLowerCase() === 'on'
}


function numericValue(signal?: TelemetrySignal) {
  return typeof signal?.value === 'number'
    ? signal.value
    : undefined
}


function hasAlarmTerm(
  alarms: Alarm[],
  terms: string[],
) {
  return alarms.some((alarm) => {
    const text = `${alarm.alarm_code ?? ''} ${alarm.source_signal ?? ''}`
      .toUpperCase()

    return terms.some((term) => text.includes(term))
  })
}


function alarmStateForTerms(
  alarms: Alarm[],
  terms: string[],
): MachineState | undefined {
  const matching = alarms.filter((alarm) => {
    const text = `${alarm.alarm_code ?? ''} ${alarm.source_signal ?? ''}`
      .toUpperCase()
    return terms.some((term) => text.includes(term))
  })

  if (matching.length === 0) return undefined

  return matching.some((alarm) => {
    const severity = alarm.severity?.toUpperCase()
    return severity === 'CRITICAL' || severity === 'HIGH'
  })
    ? 'alarm'
    : 'warning'
}


interface FlowEffectProps {
  active: boolean
  state: MachineState
  speed: number
}


interface CircuitFlowProps extends FlowEffectProps {
  points: [number, number, number][]
}


interface PipeProps {
  points: [number, number, number][]
  color: string
  radius?: number
}


function PhysicalPipe({
  points,
  color,
  radius = 0.085,
}: PipeProps) {
  const curve = useMemo(
    () => new CatmullRomCurve3(
      points.map((point) => new Vector3(...point)),
      false,
      'catmullrom',
      0.08,
    ),
    [points],
  )

  return (
    <mesh castShadow receiveShadow>
      <tubeGeometry args={[curve, 72, radius, 12, false]} />
      <meshStandardMaterial
        color={color}
        metalness={0.72}
        roughness={0.24}
        emissive={color}
        emissiveIntensity={0.08}
      />
    </mesh>
  )
}


function CircuitFlow({
  active,
  state,
  speed,
  points,
}: CircuitFlowProps) {
  const arrows = useRef<Group>(null)
  const progress = useRef(0)
  const curve = useMemo(
    () => new CatmullRomCurve3(
      points.map((point) => new Vector3(...point)),
      true,
      'catmullrom',
      0.12,
    ),
    [points],
  )

  useFrame((_, delta) => {
    if (!active || !arrows.current) return

    progress.current = (
      progress.current + delta * speed * 0.035 + 1
    ) % 1

    arrows.current.children.forEach((arrow, index) => {
      const offset = (progress.current + index / 18) % 1
      const point = curve.getPointAt(offset)
      const tangent = curve.getTangentAt(offset)

      arrow.position.copy(point)
      arrow.rotation.set(
        0,
        Math.atan2(tangent.x, tangent.z),
        Math.atan2(tangent.y, Math.hypot(tangent.x, tangent.z)),
      )
    })
  })

  if (!active) return null

  return (
    <group ref={arrows}>
      {Array.from({ length: 18 }, (_, index) => (
        <group key={index}>
          <mesh rotation={[Math.PI / 2, 0, 0]}>
            <coneGeometry args={[0.12, 0.3, 12]} />
            <meshBasicMaterial color={stateColors[state]} />
          </mesh>
          <mesh position={[0, 0, 0.17]}>
            <boxGeometry args={[0.055, 0.055, 0.25]} />
            <meshBasicMaterial color={stateColors[state]} />
          </mesh>
        </group>
      ))}
    </group>
  )
}


interface SensorMarkerProps {
  signal?: TelemetrySignal
  label: string
  position: [number, number, number]
}


function SensorMarker({
  signal,
  label,
  position,
}: SensorMarkerProps) {
  const [hovered, setHovered] = useState(false)
  const state = signal?.quality?.toLowerCase() === 'good'
    ? 'normal'
    : signal
      ? 'warning'
      : 'offline'
  const color = signal?.name?.includes('TEMP')
    ? temperatureColor(signal)
    : stateColors[state]

  return (
    <group position={position}>
      <mesh
        onPointerEnter={(event) => {
          event.stopPropagation()
          setHovered(true)
          document.body.style.cursor = 'help'
        }}
        onPointerLeave={() => {
          setHovered(false)
          document.body.style.cursor = 'auto'
        }}
      >
        <sphereGeometry args={[0.11, 16, 16]} />
        <meshStandardMaterial
          color={color}
          emissive={color}
          emissiveIntensity={0.55}
        />
      </mesh>
      {hovered && (
        <Html distanceFactor={12} position={[0, 0.32, 0]} center>
          <div className="pointer-events-none min-w-32 whitespace-nowrap rounded-lg border border-slate-200 bg-white/95 px-2.5 py-2 text-[9px] shadow-lg backdrop-blur">
            <p className="font-bold text-slate-800">{label}</p>
            <p className="mt-0.5 text-slate-600">{signalValue(signal)}</p>
            <p className="mt-0.5 capitalize text-slate-400">
              Quality: {signal?.quality ?? 'unknown'}
            </p>
          </div>
        </Html>
      )}
    </group>
  )
}


function AirFlowEffect({
  active,
  state,
  speed,
}: FlowEffectProps) {
  const particles = useRef<Group>(null)

  useFrame((_, delta) => {
    if (!active || !particles.current) return

    particles.current.children.forEach((particle) => {
      particle.position.x -= delta * speed
      particle.position.y += Math.sin(
        particle.position.x * 2.4,
      ) * delta * 0.12

      if (particle.position.x < -3.05) {
        particle.position.x = 3.05
      }
      if (particle.position.x > 3.05) {
        particle.position.x = -3.05
      }
    })
  })

  if (!active) return null

  return (
    <group ref={particles} position={[0.25, 2.7, 0.55]}>
      {Array.from({ length: 15 }, (_, index) => (
        <group
          key={index}
          position={[
            3 - (index * 0.43),
            Math.sin(index * 1.1) * 0.22,
            ((index % 3) - 1) * 0.32,
          ]}
          rotation={[0, 0, Math.PI / 2]}
        >
          <mesh>
            <coneGeometry args={[0.1, 0.26, 10]} />
            <meshBasicMaterial
              color={stateColors[state]}
              transparent
              opacity={0.82}
            />
          </mesh>
          <mesh position={[0, -0.2, 0]}>
            <boxGeometry args={[0.055, 0.24, 0.055]} />
            <meshBasicMaterial
              color={stateColors[state]}
              transparent
              opacity={0.7}
            />
          </mesh>
        </group>
      ))}
    </group>
  )
}


function SparkEffect({ active }: { active: boolean }) {
  const sparks = useRef<Group>(null)

  useFrame(({ clock }) => {
    if (!sparks.current) return

    sparks.current.rotation.z = clock.elapsedTime * 5
    sparks.current.scale.setScalar(
      0.8 + Math.abs(Math.sin(clock.elapsedTime * 14)) * 0.7,
    )
  })

  if (!active) return null

  return (
    <group ref={sparks} position={[-0.35, 1.45, 0.4]}>
      {[0, 1, 2, 3, 4, 5].map((index) => (
        <mesh
          key={index}
          rotation={[0, 0, (Math.PI * 2 * index) / 6]}
          position={[
            Math.cos((Math.PI * 2 * index) / 6) * 0.32,
            Math.sin((Math.PI * 2 * index) / 6) * 0.32,
            0,
          ]}
        >
          <boxGeometry args={[0.35, 0.025, 0.025]} />
          <meshBasicMaterial color="#facc15" />
        </mesh>
      ))}
      <pointLight color="#f97316" intensity={4} distance={2.5} />
    </group>
  )
}


function LeakEffect({ active }: { active: boolean }) {
  const puddle = useRef<Mesh>(null)
  const droplet = useRef<Mesh>(null)

  useFrame(({ clock }) => {
    if (!puddle.current) return
    const pulse = 1 + Math.sin(clock.elapsedTime * 2.2) * 0.06
    puddle.current.scale.set(pulse, pulse * 0.72, 1)

    if (droplet.current) {
      droplet.current.position.y =
        0.15 + ((1.35 - (clock.elapsedTime * 0.8) % 1.2))
    }
  })

  if (!active) return null

  return (
    <group position={[-2.65, -0.2, 1.55]}>
      <mesh ref={puddle} rotation={[-Math.PI / 2, 0, 0]}>
        <circleGeometry args={[0.85, 32]} />
        <meshPhysicalMaterial
          color="#38bdf8"
          transparent
          opacity={0.55}
          roughness={0.05}
          metalness={0.05}
        />
      </mesh>
      <mesh ref={droplet} position={[0, 1.25, -0.35]}>
        <sphereGeometry args={[0.075, 12, 12]} />
        <meshBasicMaterial color="#7dd3fc" />
      </mesh>
      <MachineLabel
        title="Leak Detected"
        value="Inspect circuit"
        state="alarm"
        position={[0, 1.25, 0]}
      />
    </group>
  )
}


interface LabelProps {
  title: string
  value: string
  state: MachineState
  position: [number, number, number]
}


function MachineLabel({
  title,
  value,
  state,
  position,
}: LabelProps) {
  return (
    <Html
      position={position}
      center
      distanceFactor={10}
      style={{ pointerEvents: 'none' }}
    >
      <div className="min-w-24 rounded-lg border border-slate-200 bg-white/92 px-2 py-1.5 text-center shadow-md backdrop-blur">
        <p className="whitespace-nowrap text-[8px] font-bold uppercase tracking-wide text-slate-500">
          {title}
        </p>
        <div className="mt-1 flex items-center justify-center gap-1.5">
          <span
            className="size-2 rounded-full"
            style={{ backgroundColor: stateColors[state] }}
          />
          <p className="whitespace-nowrap text-[10px] font-semibold text-slate-900">
            {value}
          </p>
        </div>
      </div>
    </Html>
  )
}


interface SceneProps extends DigitalTwin3DProps {
  selected: string | null
  setSelected: (value: string | null) => void
}


function CoolingPlantModel({
  online,
  signals,
  alarms,
  selected,
  setSelected,
}: SceneProps) {
  const fanOne = useRef<Group>(null)
  const fanTwo = useRef<Group>(null)
  const fanThree = useRef<Group>(null)
  const evapFanOne = useRef<Group>(null)
  const evapFanTwo = useRef<Group>(null)
  const compressorRotor = useRef<Group>(null)
  const coldRoomDoor = useRef<Group>(null)
  const alarmGlow = useRef<Mesh>(null)

  const roomTemperatures = [1, 2, 3, 4].map((number) =>
    findExactSignal(
      signals,
      `ROOM_TEMP_0${number}`,
    ),
  )
  const temperature = [...roomTemperatures]
    .filter((signal) => numericValue(signal) !== undefined)
    .sort(
      (first, second) =>
        (numericValue(second) ?? -100)
        - (numericValue(first) ?? -100),
    )[0] ?? roomTemperatures[0]
  const airTemperature = findExactSignal(signals, 'AIR_TEMP_01')
  const humidity = findExactSignal(signals, 'AIR_RH_01')
  const door = findExactSignal(signals, 'DOOR_SAFE')
  const doorAux = findExactSignal(signals, 'DOOR_AUX')
  const pressure = findSignal(signals, ['PRESS'])
  const fan = findExactSignal(signals, 'EVAP_FAN_RUN')
  const airflow = findSignal(
    signals,
    ['AIR_FLOW', 'AIRFLOW', 'AIR_SPEED', 'FLOW'],
  )
  const refrigerantFlow = findSignal(
    signals,
    ['REFRIGERANT_FLOW', 'REFRIG_FLOW'],
  )
  const powerFailure = findExactSignal(signals, 'POWER_FAILURE')
  const compressorTrip = findExactSignal(signals, 'COMPRESSOR_TRIP')
  const compressorCurrent = findExactSignal(signals, 'COMPRESSOR_CURRENT')
  const batteryVoltage = findExactSignal(signals, 'BATTERY_VOLTAGE')
  const leak = findExactSignal(signals, 'LEAK_ALARM')
  const leakCableFault = findExactSignal(signals, 'LEAK_CABLE_FAULT')
  const compressor = findExactSignal(signals, 'COMPRESSOR_RUN')

  const temperatureState = !online
    ? 'offline'
    : alarmStateForTerms(
      alarms,
      ['ROOM_TEMP', 'AIR_TEMP', 'TEMPERATURE'],
    ) ?? machineState(temperature, alarms, online)
  const pressureState = machineState(
    pressure,
    alarms,
    online,
  )
  const fanState = !online
    ? 'offline'
    : alarmStateForTerms(alarms, ['EVAP_FAN', 'FAN'])
      ?? machineState(fan, alarms, online)
  const compressorSignalState = !online
    ? 'offline'
    : alarmStateForTerms(
      alarms,
      ['COMPRESSOR_RUN', 'COMPRESSOR_TRIP', 'COMPRESSOR_CURRENT'],
    ) ?? machineState(compressor, alarms, online)

  const tripActive = isRunning(compressorTrip)
  const powerFaultActive = isRunning(powerFailure)
  const compressorState: MachineState =
    tripActive || powerFaultActive
      ? 'alarm'
      : compressorSignalState
  const compressorRunning = online
    && isRunning(compressor)
    && !tripActive
    && !powerFaultActive
  const evapFansRunning = compressorRunning && isRunning(fan)
  const condenserFansRunning = compressorRunning
  const doorOpen = online && (
    door?.value === false
    || isRunning(doorAux)
  )
  const compressorCurrentAmps = Math.max(
    0,
    numericValue(compressorCurrent) ?? 0,
  )
  const compressorMotionSpeed = Math.min(
    12,
    Math.max(2.2, compressorCurrentAmps * 0.38),
  )
  const electricalFault = online && (
    tripActive
    || powerFaultActive
    || hasAlarmTerm(
      alarms,
      ['VOLT', 'CURRENT', 'ELECTRIC', 'POWER'],
    )
  )
  const leakDetected = online && (
    isRunning(leak)
    || hasAlarmTerm(alarms, ['LEAK', 'LIQUID'])
  )
  const rawAirflowValue = numericValue(airflow) ?? 1
  const rawRefrigerantFlow = numericValue(refrigerantFlow) ?? 1
  const refrigerantSpeedMagnitude = Math.min(
    3.2,
    Math.max(
      0.65,
      Math.abs(rawRefrigerantFlow) * 0.7,
    ),
  )
  const airflowSpeedMagnitude = Math.min(
    4.5,
    Math.max(0.85, Math.abs(rawAirflowValue) * 0.6),
  )
  const refrigerantSpeed = refrigerantSpeedMagnitude
    * (rawRefrigerantFlow < 0 ? -1 : 1)
  const airflowSpeed = airflowSpeedMagnitude
    * (rawAirflowValue < 0 ? -1 : 1)
  const circuitState: MachineState =
    pressureState === 'alarm' || compressorState === 'alarm'
      ? 'alarm'
      : pressureState === 'warning' || compressorState === 'warning'
        ? 'warning'
        : !online
          ? 'offline'
          : 'normal'

  useFrame(({ clock }, delta) => {
    if (condenserFansRunning) {
      ;[fanOne, fanTwo, fanThree].forEach((reference) => {
        if (reference.current) {
          reference.current.rotation.z -= delta * 4
        }
      })
    }

    if (evapFansRunning) {
      ;[evapFanOne, evapFanTwo].forEach((reference) => {
        if (reference.current) {
          reference.current.rotation.z -= delta * 5.5
        }
      })
    }

    if (compressorRunning && compressorRotor.current) {
      compressorRotor.current.rotation.z -=
        delta * compressorMotionSpeed
    }

    if (coldRoomDoor.current) {
      const targetRotation = doorOpen ? -Math.PI * 0.42 : 0
      coldRoomDoor.current.rotation.y +=
        (targetRotation - coldRoomDoor.current.rotation.y)
        * Math.min(1, delta * 4.5)
    }

    if (alarmGlow.current && temperatureState === 'alarm') {
      const pulse = 1 + Math.sin(clock.elapsedTime * 4) * 0.08
      alarmGlow.current.scale.setScalar(pulse)
    }
  })

  const equipmentMaterial = (
    state: MachineState,
    selectedName: string,
  ) => ({
    color: stateColors[state],
    metalness: 0.38,
    roughness: 0.32,
    emissive: state === 'alarm'
      ? '#7f1d1d'
      : selected === selectedName
        ? '#075985'
        : '#000000',
    emissiveIntensity:
      state === 'alarm' || selected === selectedName
        ? 0.48
        : 0,
  })

  return (
    <group position={[0, -0.35, 0]}>
      <mesh
        receiveShadow
        position={[0, -0.35, 0]}
      >
        <boxGeometry args={[11.5, 0.22, 6.5]} />
        <meshStandardMaterial color="#e8f1f8" roughness={0.82} />
      </mesh>

      <gridHelper
        args={[11, 22, '#cbd5e1', '#e2e8f0']}
        position={[0, -0.22, 0]}
      />

      {/* Industrial service bay structure and safety markings. */}
      {[-5.35, 5.35].map((x) => [-2.85, 2.85].map((z) => (
        <mesh key={`bay-post-${x}-${z}`} position={[x, 1.65, z]}>
          <boxGeometry args={[0.16, 3.9, 0.16]} />
          <meshStandardMaterial color="#64748b" metalness={0.78} roughness={0.28} />
        </mesh>
      )))}
      {[-2.85, 2.85].map((z) => (
        <mesh key={`roof-beam-${z}`} position={[0, 3.55, z]}>
          <boxGeometry args={[10.85, 0.15, 0.18]} />
          <meshStandardMaterial color="#64748b" metalness={0.78} />
        </mesh>
      ))}
      {[-2.45, 2.45].map((z) => (
        <mesh key={`safety-${z}`} position={[0, -0.205, z]}>
          <boxGeometry args={[10.4, 0.035, 0.08]} />
          <meshStandardMaterial color="#fbbf24" emissive="#92400e" emissiveIntensity={0.08} />
        </mesh>
      ))}

      <group
        position={[-3.7, 1.05, 0.55]}
        onClick={(event) => {
          event.stopPropagation()
          setSelected('cold-room')
        }}
      >
        {/* Insulated cold-room shell: separate panels make the room readable. */}
        <mesh receiveShadow position={[0, -1.18, 0]}>
          <boxGeometry args={[2.9, 0.16, 2.8]} />
          <meshStandardMaterial color="#d7e3eb" roughness={0.58} />
        </mesh>
        <mesh castShadow receiveShadow position={[0, 0, -1.34]}>
          <boxGeometry args={[2.9, 2.55, 0.14]} />
          <meshStandardMaterial color="#edf4f8" metalness={0.2} roughness={0.42} />
        </mesh>
        <mesh castShadow receiveShadow position={[-1.38, 0, 0]}>
          <boxGeometry args={[0.14, 2.55, 2.8]} />
          <meshStandardMaterial color="#e5eef4" metalness={0.2} roughness={0.42} />
        </mesh>
        <mesh castShadow receiveShadow position={[1.38, 0, 0]}>
          <boxGeometry args={[0.14, 2.55, 2.8]} />
          <meshStandardMaterial color="#e5eef4" metalness={0.2} roughness={0.42} />
        </mesh>
        <mesh castShadow position={[0, 1.27, 0]}>
          <boxGeometry args={[2.9, 0.14, 2.8]} />
          <meshStandardMaterial color="#dbe7ee" metalness={0.25} roughness={0.36} />
        </mesh>

        {/* Live thermal volume inside the chamber. */}
        <mesh position={[0, -0.03, 0]}>
          <boxGeometry args={[2.58, 2.2, 2.45]} />
          <meshPhysicalMaterial
            color={temperatureColor(temperature)}
            emissive={temperatureColor(temperature)}
            emissiveIntensity={temperatureState === 'alarm' ? 0.24 : 0.06}
            transparent
            opacity={temperatureState === 'alarm' ? 0.2 : 0.1}
            roughness={0.1}
            transmission={0.2}
            depthWrite={false}
          />
        </mesh>

        {/* Panel seams and corner protection rails. */}
        {[-0.92, -0.46, 0, 0.46, 0.92].map((x) => (
          <mesh key={`room-seam-${x}`} position={[x, 0, -1.418]}>
            <boxGeometry args={[0.025, 2.42, 0.02]} />
            <meshStandardMaterial color="#b8c8d4" metalness={0.42} />
          </mesh>
        ))}
        {[-1.43, 1.43].map((x) => [-1.38, 1.38].map((z) => (
          <mesh key={`corner-${x}-${z}`} position={[x, 0, z]}>
            <boxGeometry args={[0.1, 2.7, 0.1]} />
            <meshStandardMaterial color="#64748b" metalness={0.72} />
          </mesh>
        )))}

        {/* Insulated doorway and animated cold-room door. */}
        <mesh position={[0, 0, 1.37]}>
          <boxGeometry args={[2.25, 1.95, 0.08]} />
          <meshStandardMaterial color="#dbe4ec" metalness={0.35} />
        </mesh>
        <group
          ref={coldRoomDoor}
          position={[-0.92, 0, 1.43]}
        >
          <mesh castShadow position={[0.92, 0, 0]}>
            <boxGeometry args={[1.82, 1.62, 0.11]} />
            <meshStandardMaterial
              color={doorOpen ? '#f59e0b' : '#f8fafc'}
              metalness={0.28}
              roughness={0.38}
              emissive={doorOpen ? '#78350f' : '#000000'}
              emissiveIntensity={doorOpen ? 0.24 : 0}
            />
          </mesh>
          <mesh position={[1.58, 0, 0.08]}>
            <boxGeometry args={[0.08, 0.46, 0.08]} />
            <meshStandardMaterial color="#334155" metalness={0.75} />
          </mesh>
          <mesh position={[0.92, 0.28, 0.07]}>
            <boxGeometry args={[0.86, 0.48, 0.04]} />
            <meshPhysicalMaterial
              color="#bfdbfe"
              transparent
              opacity={0.58}
              roughness={0.08}
              transmission={0.22}
            />
          </mesh>
          <mesh
            ref={alarmGlow}
            position={[0.92, 0, 0.09]}
            visible={temperatureState === 'alarm'}
          >
            <planeGeometry args={[1.92, 1.72]} />
            <meshBasicMaterial color="#ef4444" transparent opacity={0.22} />
          </mesh>
        </group>

        <MachineLabel
          title="Cold Room Temperature"
          value={signalValue(temperature)}
          state={temperatureState}
          position={[-0.2, 2.15, -0.45]}
        />
      </group>

      {/* Evaporator mounted inside the cold room. */}
      <group position={[-3.7, 1.65, -0.35]}>
        <mesh castShadow>
          <boxGeometry args={[2.15, 0.78, 0.64]} />
          <meshStandardMaterial color="#cbdbe7" metalness={0.48} roughness={0.3} />
        </mesh>
        {Array.from({ length: 17 }, (_, index) => (
          <mesh
            key={`evap-fin-${index}`}
            position={[-0.9 + index * 0.1125, 0, 0.332]}
          >
            <boxGeometry args={[0.025, 0.57, 0.025]} />
            <meshStandardMaterial color="#94a3b8" metalness={0.72} />
          </mesh>
        ))}
        <mesh position={[0, -0.48, 0.05]}>
          <boxGeometry args={[2.25, 0.16, 0.74]} />
          <meshStandardMaterial color="#64748b" metalness={0.6} />
        </mesh>

        {[
          { x: -0.52, reference: evapFanOne },
          { x: 0.52, reference: evapFanTwo },
        ].map(({ x, reference }) => (
          <group key={x} ref={reference} position={[x, 0, 0.31]}>
            <mesh>
              <torusGeometry args={[0.27, 0.035, 12, 32]} />
              <meshStandardMaterial color="#475569" metalness={0.65} />
            </mesh>
            {[0, Math.PI / 2, Math.PI, Math.PI * 1.5].map((rotation) => (
              <mesh
                key={rotation}
                rotation={[0, 0, rotation]}
                position={[
                  0.17 * Math.cos(rotation),
                  0.17 * Math.sin(rotation),
                  0,
                ]}
              >
                <boxGeometry args={[0.32, 0.09, 0.05]} />
                <meshStandardMaterial
                  {...equipmentMaterial(fanState, 'evaporator')}
                />
              </mesh>
            ))}
            <mesh>
              <sphereGeometry args={[0.07, 12, 12]} />
              <meshStandardMaterial color="#334155" />
            </mesh>
          </group>
        ))}
        <MachineLabel
          title="Evaporator"
          value={evapFansRunning ? 'Airflow active' : 'Fans stopped'}
          state={fanState}
          position={[0, 0.85, 0]}
        />
      </group>

      <SensorMarker signal={roomTemperatures[0]} label="T1" position={[-4.55, 1.8, 1.95]} />
      <SensorMarker signal={roomTemperatures[1]} label="T2" position={[-2.85, 1.8, 1.95]} />
      <SensorMarker signal={roomTemperatures[2]} label="T3" position={[-4.55, 0.55, 1.95]} />
      <SensorMarker signal={roomTemperatures[3]} label="T4" position={[-2.85, 0.55, 1.95]} />
      <SensorMarker signal={airTemperature} label="Air" position={[-3.7, 1.05, 2.02]} />
      <SensorMarker signal={humidity} label="RH" position={[-3.7, 0.38, 2.02]} />
      <SensorMarker signal={door} label="Door Safety" position={[-2.72, 1.05, 2.02]} />
      <SensorMarker signal={doorAux} label="Door Auxiliary" position={[-2.72, 0.65, 2.02]} />

      <group
        position={[-0.35, 0.15, 0.35]}
        onClick={(event) => {
          event.stopPropagation()
          setSelected('compressor')
        }}
      >
        <mesh castShadow position={[0, 0, 0]}>
          <boxGeometry args={[2.7, 0.32, 2.15]} />
          <meshStandardMaterial color="#64748b" metalness={0.58} roughness={0.32} />
        </mesh>

        {[-0.78, 0, 0.78].map((x) => (
          <group key={x} position={[x, 0.72, 0]}>
            <mesh castShadow rotation={[0, 0, Math.PI / 2]}>
              <cylinderGeometry args={[0.42, 0.42, 1.05, 32]} />
              <meshStandardMaterial {...equipmentMaterial(compressorState, 'compressor')} />
            </mesh>
            <mesh castShadow position={[0, 0.46, 0]}>
              <boxGeometry args={[0.55, 0.38, 0.72]} />
              <meshStandardMaterial {...equipmentMaterial(compressorState, 'compressor')} />
            </mesh>
          </group>
        ))}

        <group ref={compressorRotor} position={[0, 0.72, 1.12]}>
          <mesh>
            <torusGeometry args={[0.34, 0.075, 14, 36]} />
            <meshStandardMaterial
              color={stateColors[compressorState]}
              metalness={0.7}
              roughness={0.25}
              emissive={compressorState === 'alarm' ? '#7f1d1d' : '#000000'}
              emissiveIntensity={compressorState === 'alarm' ? 0.6 : 0}
            />
          </mesh>
          {[0, Math.PI / 2, Math.PI, Math.PI * 1.5].map((rotation) => (
            <mesh key={rotation} rotation={[0, 0, rotation]}>
              <boxGeometry args={[0.62, 0.07, 0.07]} />
              <meshStandardMaterial color="#475569" metalness={0.7} />
            </mesh>
          ))}
          <mesh>
            <cylinderGeometry args={[0.1, 0.1, 0.18, 20]} />
            <meshStandardMaterial color="#1e293b" metalness={0.8} />
          </mesh>
        </group>

        <MachineLabel
          title="Compressor Rack"
          value={compressorRunning
            ? `${compressorCurrentAmps.toFixed(1)} A · Running`
            : 'Stopped'}
          state={compressorState}
          position={[0, 2.05, -0.65]}
        />
      </group>

      <group
        position={[3.65, 1.1, 0.15]}
        onClick={(event) => {
          event.stopPropagation()
          setSelected('condenser')
        }}
      >
        <mesh castShadow>
          <boxGeometry args={[2.8, 2.2, 1.2]} />
          <meshStandardMaterial
            color="#dbeafe"
            metalness={0.24}
            roughness={0.36}
          />
        </mesh>

        {Array.from({ length: 13 }, (_, index) => (
          <mesh
            key={`condenser-fin-${index}`}
            position={[-1.18 + index * 0.197, -0.48, 0.615]}
          >
            <boxGeometry args={[0.035, 0.72, 0.035]} />
            <meshStandardMaterial color="#94a3b8" metalness={0.66} />
          </mesh>
        ))}

        {[
          { x: -0.82, reference: fanOne },
          { x: 0, reference: fanTwo },
          { x: 0.82, reference: fanThree },
        ].map(({ x, reference }) => (
          <group
            key={x}
            ref={reference}
            position={[x, 0.24, 0.63]}
          >
            <mesh>
              <torusGeometry args={[0.34, 0.055, 12, 32]} />
              <meshStandardMaterial color={stateColors[compressorState]} />
            </mesh>
            {[0, Math.PI / 2, Math.PI, Math.PI * 1.5].map((rotation) => (
              <mesh key={rotation} rotation={[0, 0, rotation]} position={[0.18 * Math.cos(rotation), 0.18 * Math.sin(rotation), 0]}>
                <boxGeometry args={[0.34, 0.1, 0.06]} />
                <meshStandardMaterial {...equipmentMaterial(compressorState, 'condenser')} />
              </mesh>
            ))}
            <mesh>
              <sphereGeometry args={[0.08, 16, 16]} />
              <meshStandardMaterial color="#334155" />
            </mesh>
          </group>
        ))}

        <MachineLabel
          title="Condenser Fans"
          value={condenserFansRunning ? 'Running' : 'Stopped'}
          state={compressorState}
          position={[0.25, 2.15, -0.35]}
        />
      </group>

      {/* Physical refrigeration circuit: hot-gas, liquid and suction lines. */}
      <PhysicalPipe
        points={[
          [-0.35, 1.32, -0.55],
          [0.9, 1.65, -1.4],
          [2.25, 1.65, -1.0],
          [3.15, 1.4, -0.72],
        ]}
        color={circuitState === 'alarm' ? '#ef4444' : '#f97316'}
        radius={0.11}
      />
      <PhysicalPipe
        points={[
          [3.75, 0.38, 0.72],
          [3.12, 0.25, 1.85],
          [1.1, 0.28, 2.05],
          [-1.35, 0.28, 2.05],
          [-3.08, 0.5, 1.72],
        ]}
        color={circuitState === 'alarm' ? '#ef4444' : '#eab308'}
        radius={0.075}
      />
      <PhysicalPipe
        points={[
          [-3.25, 1.5, -0.42],
          [-2.45, 0.62, -1.7],
          [-1.35, 0.55, -1.82],
          [-0.38, 0.9, -0.75],
        ]}
        color={circuitState === 'alarm' ? '#ef4444' : '#38bdf8'}
        radius={0.13}
      />

      {/* Liquid receiver and expansion valve complete the refrigeration loop. */}
      <group position={[2.35, 0.55, 1.7]}>
        <mesh castShadow>
          <cylinderGeometry args={[0.25, 0.25, 1.05, 24]} />
          <meshStandardMaterial
            color={stateColors[circuitState]}
            metalness={0.55}
            roughness={0.28}
          />
        </mesh>
        <MachineLabel
          title="Liquid Receiver"
          value={compressorRunning ? 'Flow active' : 'No flow'}
          state={circuitState}
          position={[0, 1.05, 0]}
        />
      </group>

      <group position={[-3.08, 0.5, 1.72]}>
        <mesh rotation={[0, 0, Math.PI / 4]}>
          <octahedronGeometry args={[0.24]} />
          <meshStandardMaterial
            color="#f59e0b"
            metalness={0.4}
          />
        </mesh>
        <MachineLabel
          title="Expansion Valve"
          value={compressorRunning ? 'Metering' : 'Closed'}
          state={circuitState}
          position={[0, 0.75, 0]}
        />
      </group>

      <CircuitFlow
        active={compressorRunning}
        state={circuitState}
        speed={refrigerantSpeed}
        points={[
          [-0.35, 1.28, -0.6],
          [2.25, 1.65, -1.0],
          [3.65, 1.1, -0.85],
          [3.15, 0.28, 1.85],
          [-3.15, 0.35, 1.85],
          [-3.7, 1.65, -0.35],
          [-2.35, 0.55, -1.75],
          [-0.35, 0.55, -1.75],
        ]}
      />

      <AirFlowEffect
        active={evapFansRunning}
        state={fanState}
        speed={airflowSpeed}
      />

      <SparkEffect active={electricalFault} />
      <LeakEffect active={leakDetected} />

      {/* Electrical control cabinet. POWER_FAILURE darkens all equipment. */}
      <group position={[1.15, 0.72, -2.15]}>
        <mesh castShadow>
          <boxGeometry args={[1.15, 1.55, 0.48]} />
          <meshStandardMaterial
            color={powerFaultActive ? '#334155' : '#e2e8f0'}
            metalness={0.48}
            roughness={0.34}
            emissive={powerFaultActive ? '#7f1d1d' : '#000000'}
            emissiveIntensity={powerFaultActive ? 0.65 : 0}
          />
        </mesh>
        <mesh position={[0, 0.25, 0.255]}>
          <boxGeometry args={[0.7, 0.42, 0.035]} />
          <meshBasicMaterial color={powerFaultActive ? '#0f172a' : '#0ea5e9'} />
        </mesh>
        <pointLight
          position={[0.38, -0.48, 0.42]}
          color={powerFaultActive ? '#ef4444' : '#22c55e'}
          intensity={powerFaultActive ? 2.6 : 1.2}
          distance={1.8}
        />
        <MachineLabel
          title="Electrical Panel"
          value={powerFaultActive ? 'Power failure' : 'Energized'}
          state={powerFaultActive ? 'alarm' : online ? 'normal' : 'offline'}
          position={[0, 1.25, 0]}
        />
      </group>

      <group position={[1.55, 2.2, 0.2]}>
        <mesh rotation={[Math.PI / 2, 0, 0]}>
          <cylinderGeometry args={[0.47, 0.47, 0.16, 32]} />
          <meshStandardMaterial color="#ffffff" />
        </mesh>
        <mesh position={[0, 0, 0.1]}>
          <circleGeometry args={[0.38, 32]} />
          <meshStandardMaterial color="#f8fafc" />
        </mesh>
        <mesh position={[0, 0, 0.16]} rotation={[0, 0, -0.72]}>
          <boxGeometry args={[0.06, 0.29, 0.04]} />
          <meshStandardMaterial color={stateColors[pressureState]} />
        </mesh>
        <MachineLabel
          title="System Pressure"
          value={pressure ? signalValue(pressure) : 'Not instrumented'}
          state={pressureState}
          position={[0, 0.95, 0]}
        />
      </group>

      <SensorMarker
        signal={compressorCurrent}
        label="Compressor Current"
        position={[-0.35, 1.45, 1.25]}
      />
      <SensorMarker
        signal={batteryVoltage}
        label="Battery"
        position={[0.65, 1.25, 1.2]}
      />
      <SensorMarker signal={compressor} label="Compressor Run" position={[-1.1, 1.35, 1.2]} />
      <SensorMarker signal={compressorTrip} label="Compressor Trip" position={[-0.72, 1.65, 1.15]} />
      <SensorMarker signal={fan} label="Evaporator Fan" position={[-3.7, 2.08, 0.02]} />
      <SensorMarker signal={leak} label="Leak Alarm" position={[-2.65, 0.3, 1.58]} />
      <SensorMarker signal={leakCableFault} label="Leak Cable" position={[-2.25, 0.28, 1.55]} />
      <SensorMarker signal={powerFailure} label="Power Failure" position={[1.48, 0.35, -1.88]} />
    </group>
  )
}


function DigitalTwin3D({
  online,
  signals,
  alarms,
}: DigitalTwin3DProps) {
  const [selected, setSelected] =
    useState<string | null>(null)

  return (
    <div className="relative h-[520px] overflow-hidden rounded-3xl border border-slate-200 bg-gradient-to-br from-white via-sky-50/50 to-blue-50 shadow-inner sm:h-[610px]">
      <div className="absolute left-5 top-5 z-10 rounded-xl border border-slate-200 bg-white/90 px-4 py-3 shadow-sm backdrop-blur">
        <p className="text-xs font-bold uppercase tracking-[0.18em] text-cyan-600">
          Live Digital Twin
        </p>
        <p className="mt-1 text-xs text-slate-500">
          Drag to rotate · Scroll to zoom · Select equipment
        </p>
      </div>

      <Canvas
        shadows
        dpr={[1, 1.5]}
        camera={{
          position: [9.5, 7.2, 10.8],
          fov: 38,
        }}
        onPointerMissed={() => setSelected(null)}
      >
        <color attach="background" args={['#f8fbff']} />
        <fog attach="fog" args={['#f8fbff', 14, 27]} />

        <ambientLight intensity={1.65} />
        <hemisphereLight
          color="#ffffff"
          groundColor="#bfdbfe"
          intensity={1.15}
        />
        <directionalLight
          castShadow
          position={[7, 11, 6]}
          intensity={2.2}
          color="#ffffff"
          shadow-mapSize-width={1024}
          shadow-mapSize-height={1024}
        />
        <pointLight
          position={[-6, 4, 5]}
          intensity={0.7}
          color="#67e8f9"
        />

        <Suspense fallback={null}>
          <CoolingPlantModel
            online={online}
            signals={signals}
            alarms={alarms}
            selected={selected}
            setSelected={setSelected}
          />
        </Suspense>

        <OrbitControls
          makeDefault
          enablePan={false}
          minDistance={9}
          maxDistance={18}
          minPolarAngle={Math.PI / 5}
          maxPolarAngle={Math.PI / 2.18}
          target={[0, 0.65, 0]}
        />
      </Canvas>

      <div className="pointer-events-none absolute bottom-5 left-5 z-10 flex flex-wrap gap-2">
        {[
          ['Normal', stateColors.normal],
          ['Warning', stateColors.warning],
          ['Alarm', stateColors.alarm],
          ['Offline', stateColors.offline],
        ].map(([label, color]) => (
          <span
            key={label}
            className="flex items-center gap-2 rounded-lg border border-slate-200 bg-white/90 px-3 py-2 text-xs font-medium text-slate-600 shadow-sm backdrop-blur"
          >
            <span
              className="size-2.5 rounded-full"
              style={{ backgroundColor: color }}
            />
            {label}
          </span>
        ))}
      </div>

      <div className="pointer-events-none absolute bottom-5 right-5 z-10 hidden max-w-xs rounded-xl border border-slate-200 bg-white/92 px-3 py-2.5 shadow-lg backdrop-blur sm:block">
        {selected ? (
          <>
            <p className="text-[10px] font-bold uppercase tracking-[0.16em] text-cyan-600">
              Selected equipment
            </p>
            <p className="mt-1 text-sm font-semibold capitalize text-slate-900">
              {selected.replace('-', ' ')}
            </p>
            <p className="mt-1 text-[10px] text-slate-500">
              Hover sensor points to inspect live value and quality.
            </p>
          </>
        ) : (
          <div className="flex items-center gap-3 text-[10px] text-slate-500">
            <span><b className="text-orange-500">Orange</b> hot gas</span>
            <span><b className="text-yellow-500">Yellow</b> liquid</span>
            <span><b className="text-sky-500">Blue</b> suction</span>
          </div>
        )}
      </div>
    </div>
  )
}


export default DigitalTwin3D
