import {
  Navigate,
  Route,
  Routes,
} from 'react-router-dom'

import DashboardLayout from './layouts/DashboardLayout'
import PlaceholderPage from './pages/PlaceholderPage'
import DashboardPage from './pages/DashboardPage'
import DevicesPage from './pages/DevicesPage'
import DeviceDetailsPage from './pages/DeviceDetailsPage'
import TelemetryPage from './pages/TelemetryPage'
import ActiveAlarmsPage from './pages/ActiveAlarmsPage'
import DeviceHealthPage from './pages/DeviceHealthPage'


function App() {
  return (
    <Routes>
      <Route element={<DashboardLayout />}>
        <Route
          index
          element={<DashboardPage />}
        />

        <Route
          path="devices"
          element={<DevicesPage />}
        />

        <Route
          path="devices/:deviceId"
          element={<DeviceDetailsPage />}
        />

        <Route
          path="telemetry"
          element={<TelemetryPage />}
        />

        <Route
          path="alarms"
          element={<ActiveAlarmsPage />}
        />

        <Route
          path="health"
          element={<DeviceHealthPage />}
        />

        <Route
          path="settings"
          element={
            <PlaceholderPage
              title="Settings"
              description="Configure dashboard and alert preferences."
            />
          }
        />
      </Route>

      <Route
        path="*"
        element={
          <Navigate
            to="/"
            replace
          />
        }
      />
    </Routes>
  )
}


export default App