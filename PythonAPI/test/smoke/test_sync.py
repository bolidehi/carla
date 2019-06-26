# Copyright (c) 2019 Computer Vision Center (CVC) at the Universitat Autonoma de
# Barcelona (UAB).
#
# This work is licensed under the terms of the MIT license.
# For a copy, see <https://opensource.org/licenses/MIT>.

from . import SyncSmokeTest

import carla

try:
    import queue
except ImportError:
    import Queue as queue


class TestSynchronousMode(SyncSmokeTest):
    def test_reloading_map(self):
        settings = carla.WorldSettings(
            no_rendering_mode=False,
            synchronous_mode=True)
        for _ in range(0, 4):
            self.world = self.client.reload_world()
            self.world.apply_settings(settings)

    def test_camera_on_synchronous_mode(self):
        cam_bp = self.world.get_blueprint_library().find('sensor.camera.rgb')
        t = carla.Transform(carla.Location(z=10))
        camera = self.world.spawn_actor(cam_bp, t)
        try:

            image_queue = queue.Queue()
            camera.listen(image_queue.put)

            frame = None

            for _ in range(0, 100):
                self.world.tick()
                ts = self.world.wait_for_tick().timestamp

                if frame is not None:
                    self.assertEqual(ts.frame_count, frame + 1)

                frame = ts.frame_count

                image = image_queue.get()
                self.assertEqual(image.frame_number, ts.frame_count)
                self.assertEqual(image.timestamp, ts.elapsed_seconds)

        finally:
            camera.destroy()
