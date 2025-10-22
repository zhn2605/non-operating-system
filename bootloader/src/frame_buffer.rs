use log::{info, error};
use uefi::{
    boot::{self, ScopedProtocol, OpenProtocolParams, OpenProtocolAttributes},
    Handle,
    proto::console::gop::{GraphicsOutput, Mode}, 
    Identify, 
    Status
};

#[derive(Debug)]
pub struct FrameBuffer {
    gop: ScopedProtocol<GraphicsOutput>,
    mode: Mode,
}

impl FrameBuffer {
    pub fn init() -> Result<Self, Status> {
        info!("Started initializing..");

        // Locate all gop handles, assuming single GPU for now
        let gop_handle = match boot::get_handle_for_protocol::<GraphicsOutput>() {
            Ok(g) => {
                info!("Successfully got GOP handle! {:?}",   g);
                g
            }
            Err(e) => {
                error!("Failed to get handle: {:?}", e.status());
                return Err(e.status());
            }
        };
        
        // Multiple protocols are open why do you do this to me uefi firmware
        let gop_protocols = boot::protocols_per_handle(gop_handle).unwrap();
        info!("handle {:?} protocols: {}", gop_handle, gop_protocols.len());

        let params = OpenProtocolParams {
            handle : gop_handle,
            agent : boot::image_handle(),
            controller : None,
        };

        let gop = unsafe { match boot::open_protocol::<GraphicsOutput>(params, OpenProtocolAttributes::GetProtocol) {
            Ok(g) => {
                info!("Successfully opened GOP protocol!");
                g
            }
            Err(e) => {
                error!("Failed to open GraphicsOutput protocol for handle {:?}: {:?}", gop_handle, e.status());
                return Err(e.status());
            }
        } };

        // List all modes
        let mut max_size = 0;
        let mut max_mode: Option<Mode> = None;

        for (i, mode) in gop.modes().enumerate() {
            // Choose largest pixel screen mode for now
            let info = mode.info();
            let (w, h) = info.resolution();
            
            let size: usize = w * h;
            if size > max_size {
                max_size = size;
                max_mode = Some(mode);
            }

            let fmt = info.pixel_format();
            let stride = info.stride();
            info!("Mode {}: {}x{} {:?}, stride={}\n", i, w, h, fmt, stride);
        }
        

        // Select the current mode (you could also choose a specific one)
        // let current_mode = gop.Mode;
        Ok(Self {
            gop,
            mode: max_mode.unwrap(),
        })
    }

    // fn get_gop() -> Result<ScopedProtocol<GraphicsOutput>, Status> {

    // }
}