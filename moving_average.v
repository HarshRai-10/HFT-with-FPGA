module moving_average #(
    parameter N = 16   //Duration of moving average
)(
    input  wire        s_aclk,
    input  wire        s_aresetn,
    input  wire        s_axis_tvalid,
    input  wire        s_axis_tlast,
    input  wire [31:0] s_axis_tdata,
    output reg         s_axis_tready,
    input  wire        m_axis_tready,
    output reg         m_axis_tvalid,
    output reg [31:0]  m_axis_tdata,
    output reg         m_axis_tlast
);

    // Storage buffers
    reg [31:0] samples [0:N-1];
    reg [$clog2(N)-1:0] wr_ptr;
    reg [31+ $clog2(N):0] sum;
    
    reg [31:0] inData;
    reg isLast;

    integer i;
    integer state;

    always @(posedge s_aclk) begin
        if (!s_aresetn) begin //RESET
            wr_ptr    <= 0;
            sum       <= 0;
            s_axis_tready <= 0;
            m_axis_tdata  <= 0;
            m_axis_tvalid <= 0;
            m_axis_tlast <= 0;
            state <= 0;
            isLast <= 0;

            for (i = 0; i < N; i = i+1)
                samples[i] <= 0;

        end else case(state)
			0: begin //IDLE
				m_axis_tvalid <= 0;
				if (s_axis_tvalid) begin
					m_axis_tlast <= 0;
					m_axis_tdata <= 0;
					isLast <= s_axis_tlast;
					s_axis_tready <= 1;
					inData <= s_axis_tdata;
					state <= 1;
				end
			end
			1: begin //DOING
				s_axis_tready <= 0;
				sum = sum - samples[wr_ptr] + inData;
				samples[wr_ptr] = inData;
				
				if(wr_ptr == N-1)
					wr_ptr <= 0;
				else
					wr_ptr <= wr_ptr + 1'b1;
				
				m_axis_tdata  = sum >> $clog2(N);
				state <= 2;
			end
			2: begin //DONE
				m_axis_tvalid <= 1;
				m_axis_tlast <= isLast;
				if(m_axis_tready) begin
					state <= 0;
				end
			end
        endcase
    end

endmodule
